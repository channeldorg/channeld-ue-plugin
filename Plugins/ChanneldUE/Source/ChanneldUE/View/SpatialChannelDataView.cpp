// Fill out your copyright notice in the Description page of Project Settings.


#include "View/SpatialChannelDataView.h"

#include "ChanneldGameInstanceSubsystem.h"
#include "ChanneldNetConnection.h"
#include "ChanneldNetDriver.h"
#include "ChanneldUtils.h"
#include "Net/DataChannel.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

USpatialChannelDataView::USpatialChannelDataView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UChanneldNetConnection* USpatialChannelDataView::CreateClientConnection(ConnectionId ConnId, ChannelId ChId)
{
	ensureMsgf(!ClientInChannels.Contains(ConnId), TEXT("Client conn %d had already been added to this server"), ConnId);

	if (auto NetDriver = GetChanneldSubsystem()->GetNetDriver())//NetDriver.IsValid())
	{
		UChanneldNetConnection* ClientConn = NetDriver->AddChanneldClientConnection(ConnId, ChId);
		// Create the ControlChannel and set OpenAcked = 1
		UChannel* ControlChannel = ClientConn->CreateChannelByName(NAME_Control, EChannelCreateFlags::OpenedLocally);
		ControlChannel->OpenAcked = 1;
		ControlChannel->OpenPacketId.First = 0;
		ControlChannel->OpenPacketId.Last = 0;
		
		return ClientConn;
	}
	else
	{
		UE_LOG(LogChanneld, Error, TEXT("Spatial server failed to create client connection %d: NetDriver is not valid."), ConnId);
	}
	
	return nullptr;
}

void USpatialChannelDataView::InitPlayerController(UChanneldNetConnection* ClientConn, APlayerController* const NewPlayerController)
{
	if (AGameModeBase* const GameMode = GetWorld()->GetAuthGameMode())
	{
		// ~ Begin copy of AGameModeBase::InitNewPlayer
		// Register the player with the session
		GameMode->GameSession->RegisterPlayer(NewPlayerController, ClientConn->PlayerId.GetUniqueNetId(), false);
		GameMode->ChangeName(NewPlayerController, FString::Printf(TEXT("Player_%d"), ClientConn->GetConnId()), false);
		// ~ End copy of AGameModeBase::InitNewPlayer
		
		// ~ End copy of AGameModeBase::Login
		
		// Possess the newly-spawned player.
		NewPlayerController->NetPlayerIndex = 0;
		NewPlayerController->SetRole(ROLE_Authority);
		NewPlayerController->SetReplicates(true);
		NewPlayerController->SetPlayer(ClientConn);

		ClientConn->SetClientLoginState(EClientLoginState::ReceivedJoin);
	}
}

void USpatialChannelDataView::CreatePlayerController(UChanneldNetConnection* ClientConn)
{
	FURL InURL( NULL, *ClientConn->RequestURL, TRAVEL_Absolute );
	FString ErrorMsg;
	/* SpawnPlayActor will create the PC AND the Pawn. We need to skip some process that involves player login.
	ClientConn->PlayerController = GetWorld()->SpawnPlayActor( ClientConn, ROLE_AutonomousProxy, InURL, ClientConn->PlayerId, ErrorMsg );
	*/

	//~ Begin copy of UWorld::SpawnPlayActor
	if (AGameModeBase* const GameMode = GetWorld()->GetAuthGameMode())
	{
		// Make the option string.
		FString Options;
		for (int32 i = 0; i < InURL.Op.Num(); i++)
		{
			Options += TEXT('?');
			Options += InURL.Op[i];
		}

		// ~ Begin copy of AGameModeBase::Login
		FString ErrorMessage = GameMode->GameSession->ApproveLogin(Options);
		if (!ErrorMessage.IsEmpty())
		{
			UE_LOG(LogChanneld, Error, TEXT("Failed to create player controller for conn %d, error: %s"), ClientConn->GetConnId(), *ErrorMessage);
			return;
		}
		
		APlayerController* const NewPlayerController = GameMode->SpawnPlayerController(ENetRole::ROLE_AutonomousProxy, Options);
		if (NewPlayerController == nullptr)
		{
			UE_LOG(LogGameMode, Log, TEXT("Login: Couldn't spawn player controller of class %s"), GameMode->PlayerControllerClass ? *GameMode->PlayerControllerClass->GetName() : TEXT("NULL"));
			return;
		}

		InitPlayerController(ClientConn, NewPlayerController);
	}
	//~ End copy of UWorld::SpawnPlayActor
}

void USpatialChannelDataView::ServerHandleGetHandoverContext(UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto GetContextMsg = static_cast<const unrealpb::GetHandoverContextMessage*>(Msg);
	const auto NetId = FNetworkGUID(GetContextMsg->netid());
	UObject* Obj = GetObjectFromNetGUID(NetId);
	if (Obj == nullptr)
	{
		UE_LOG(LogChanneld, Error, TEXT("Unable to get handover context, netId: %d"), NetId.Value);
		return;
	}

	unrealpb::GetHandoverContextResultMessage ResultMsg;
	ResultMsg.set_netid(NetId.Value);
	// Group Pawn, PlayerController, and PlayerState together to make them handover at the same time.
	if (Obj->IsA<APawn>())
	{
		APawn* Pawn = Cast<APawn>(Obj);
		auto NetConn = Cast<UChanneldNetConnection>(Pawn->GetNetConnection());
		auto PawnContext = ResultMsg.add_context();
		PawnContext->mutable_obj()->CopyFrom(ChanneldUtils::GetRefOfObject(Pawn, NetConn));
		if (NetConn)
		{
			PawnContext->set_clientconnid(NetConn->GetConnId());
		}
		
		if (auto PC = Pawn->GetController())
		{
			auto PCContext = ResultMsg.add_context();
			PCContext->mutable_obj()->CopyFrom(ChanneldUtils::GetRefOfObject(PC, NetConn));
			if (NetConn)
			{
				PCContext->set_clientconnid(NetConn->GetConnId());
			}
		}

		if (auto PS = Pawn->GetPlayerState())
		{
			auto PSContext = ResultMsg.add_context();
			PSContext->mutable_obj()->CopyFrom(ChanneldUtils::GetRefOfObject(PS, NetConn));
			if (NetConn)
			{
				PSContext->set_clientconnid(NetConn->GetConnId());
			}
		}
	}

	UE_LOG(LogChanneld, Verbose, TEXT("[Server] GetHandoverContext for channeld: %s"), UTF8_TO_TCHAR(ResultMsg.DebugString().c_str()));
	
	Connection->Send(ChId, unrealpb::HANDOVER_CONTEXT, ResultMsg);
}

void USpatialChannelDataView::ServerHandleHandover(UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto HandoverMsg = static_cast<const channeldpb::ChannelDataHandoverMessage*>(Msg);
	unrealpb::HandoverData HandoverData;
	HandoverMsg->data().UnpackTo(&HandoverData);
	
	TArray<FString> NetIds;
	for (auto& HandoverContext : HandoverData.context())
	{
		NetIds.Add(FString::FromInt(HandoverContext.obj().netguid()));
	}
	UE_LOG(LogChanneld, Log, TEXT("ChannelDataHandover from channel %d to %d, object netIds: %s"), HandoverMsg->srcchannelid(), HandoverMsg->dstchannelid(), *FString::Join(NetIds, TEXT(",")));

	// The actors are handed over to current (destination) server.
	TArray<AActor*> HandoverActors;
	
	for (auto& HandoverContext : HandoverData.context())
	{
		const unrealpb::UnrealObjectRef HandoverObjRef = HandoverContext.obj();
		FNetworkGUID NetId(HandoverObjRef.netguid());
	
		// Source spatial server - the channel data is handed over from
		if (Connection->SubscribedChannels.Contains(HandoverMsg->srcchannelid()))
		{
			AActor* HandoverActor = Cast<AActor>(GetObjectFromNetGUID(NetId));
			
			// If the handover actor is no longer in the interest area of current server, delete it.
			if (!Connection->SubscribedChannels.Contains(HandoverMsg->dstchannelid()))
			{
				if (HandoverActor)
				{
					UE_LOG(LogChanneld, Log, TEXT("[Server] Deleting actor %s as it leaves the interest area"), *HandoverActor->GetName());
					if (auto HandoverPC = Cast<APlayerController>(HandoverActor))
					{
						HandoverPC->Player = nullptr;
						// Don't remove the client connection for now - we don't want to send CloseBunch to the client.
						// Keep the client connection until the client leaves the game, so we can reuse it for handover.
						HandoverPC->NetConnection = nullptr;
					}
					GetWorld()->DestroyActor(HandoverActor, true);
				}

				NetIdOwningChannels.Remove(NetId);
			}
			// If the handover actor is no longer in the authority area of current server,
			// make sure it no longer sends ChannelDataUpdate message.
			else if (!Connection->OwnedChannels.Contains(HandoverMsg->dstchannelid()))
			{
				if (HandoverActor)
				{
					HandoverActor->SetRole(ENetRole::ROLE_SimulatedProxy);
				}
			}
		}
		
		
		// Destination spatial server - the channel data is handed over to
		if (Connection->SubscribedChannels.Contains(HandoverMsg->dstchannelid()))
		{
			// Set the NetId-ChannelId mapping before spawn the object, so AddProviderToDefaultChannel won't have to query the spatial channel.
			SetOwningChannelId(NetId, HandoverMsg->dstchannelid());
		
			UObject* HandoverObj = GetObjectFromNetGUID(NetId);
			// Player enters the server - only Pawn, PlayerController or PlayerState has clientConnId > 0
			if (HandoverContext.clientconnid() > 0)
			{
				const ConnectionId ClientConnId = HandoverContext.clientconnid();
			
				// Try to spawn the object
				if (HandoverObj == nullptr)
				{
					// We need to know which client connection causes the handover, in order to spawn the object. 
					UChanneldNetConnection* ClientConn;
					if (ClientInChannels.Contains(ClientConnId))
					{
						// Update the channelId for LowLevelSend()
						ClientInChannels.Emplace(ClientConnId, HandoverMsg->dstchannelid());
						UE_LOG(LogChanneld, Log, TEXT("[Server] Updated mapping of connId: %d -> channelId: %d"), ClientConnId, HandoverMsg->dstchannelid());
						
						ClientConn = GetChanneldSubsystem()->GetNetDriver()->GetClientConnection(ClientConnId);
					}
					else
					{
						// Create the client connection if it doesn't exist yet. Don't create the PlayerController for now.
						ClientConn = CreateClientConnection(ClientConnId, HandoverMsg->dstchannelid());
					}

					const bool bHasAuthority = Connection->OwnedChannels.Contains(HandoverMsg->dstchannelid());
				
					// Set the NetId of the Pawn as exported, so it won't send the Spawn message of the Pawn to the client.
					ClientConn->SetSentSpawned(NetId);

					// HACK: turn off AddProviderToDefaultChannel() temporarily, as the NetId of the handover object may mismatch when just created. (ChanneldUtils@L656)
					bSuppressAddProviderOnServerSpawn = true;
					HandoverObj = ChanneldUtils::GetObjectByRef(&HandoverObjRef, GetWorld(), true, ClientConn);
					bSuppressAddProviderOnServerSpawn = false;
					UE_LOG(LogChanneld, Log, TEXT("[Server] Spawned handover obj: %s, clientConnId: %d"), *GetNameSafe(HandoverObj), ClientConnId);

					if (HandoverObj)
					{
						// Now the NetId is properly set, call AddProviderToDefaultChannel().
						OnServerSpawnedObject(HandoverObj, NetId);

						if (HandoverObj->IsA<APlayerController>())
						{
							InitPlayerController(ClientConn, Cast<APlayerController>(HandoverObj));
						}
						/*
						// When the pawn moves into the authority area of this server, create the PlayerController and posses the pawn.
						if (HandoverObj->IsA<APawn>() && bHasAuthority)
						{
							if (ClientConn->PlayerController == nullptr)
							{
								// FIXME: the NetId won't match the client's, causing RPC failure.
								CreatePlayerController(ClientConn);
								UE_LOG(LogChanneld, Log, TEXT("[Server] Create PlayerController for client conn %d"), ClientConn->GetConnId());
							}
							if (ClientConn->PlayerController)
							{
								ClientConn->PlayerController->Possess(Cast<APawn>(HandoverObj));
								UE_LOG(LogChanneld, Log, TEXT("[Server] PlayerController possessed pawn %s"), *HandoverObj->GetName());
							}
						}
						*/
					}
				}
			}
			// Non-player object enters the server. Simply create it.
			else
			{
				HandoverObj = ChanneldUtils::GetObjectByRef(&HandoverObjRef, GetWorld(), true);
			}
		
			if (HandoverObj)
			{
				if (AActor* HandoverActor = Cast<AActor>(HandoverObj))
				{
					// Set the role to SimulatedProxy so the actor can be updated by the handover channel data later.
					HandoverActor->SetRole(ROLE_SimulatedProxy);
					HandoverActors.Add(HandoverActor);
				}
				
				// The srcChannel and dstChannel are in the same server
				if (Connection->SubscribedChannels.Contains(HandoverMsg->srcchannelid()))
				{
					// Move data provider to the new channel
					if (HandoverObj->Implements<UChannelDataProvider>())
					{
						MoveProvider(HandoverMsg->srcchannelid(), HandoverMsg->dstchannelid(), Cast<IChannelDataProvider>(HandoverObj));
					}
					else if (HandoverObj->IsA<AActor>())
					{
						for (auto& Comp : Cast<AActor>(HandoverObj)->GetComponentsByInterface(UChannelDataProvider::StaticClass()))
						{
							MoveProvider(HandoverMsg->srcchannelid(), HandoverMsg->dstchannelid(), Cast<IChannelDataProvider>(Comp));
						}
					}				
				}
			}
			else
			{
				UE_LOG(LogChanneld, Warning, TEXT("[Server] Failed to spawn object from handover data: %s"), UTF8_TO_TCHAR(HandoverData.DebugString().c_str()));
			}
		}
	}

	// Applies the channel data update to the newly spawned objects.
	// The references between the Pawn, PlayerController, and PlayerState should be set properly in this step.
	if (HandoverActors.Num() > 0 && HandoverData.has_channeldata())
	{
		channeldpb::ChannelDataUpdateMessage UpdateMsg;
		UpdateMsg.set_allocated_data(HandoverData.mutable_channeldata());
		HandleChannelDataUpdate(_, ChId, &UpdateMsg);
	}

	// Post handover - set the actors' properties as same as they were in the source server.
	for (auto HandoverActor : HandoverActors)
	{
		// If the handover actor falls into the authority area of current server,
		// make sure it starts sending ChannelDataUpdate message. It has to be done AFTER calling HandleChannelDataUpdate.
		if (Connection->OwnedChannels.Contains(HandoverMsg->dstchannelid()))
		{
			HandoverActor->SetRole(ENetRole::ROLE_Authority);
		}

		if (auto HandoverPC = Cast<APlayerController>(HandoverActor))
		{
			HandoverPC->ServerAcknowledgePossession(HandoverPC->GetPawn());
		}
	}
}

void USpatialChannelDataView::ServerHandleSubToChannel(UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg)
{
	const auto SubResultMsg = static_cast<const channeldpb::SubscribedToChannelResultMessage*>(Msg);
	if (SubResultMsg->channeltype() == channeldpb::SPATIAL /*&& SubResultMsg->suboptions().dataaccess() == channeldpb::WRITE_ACCESS*/)
	{
		// A client is subscribed to a spatial channel the server owns (Sub message is sent by Master server)
		if (SubResultMsg->conntype() == channeldpb::CLIENT)
		{
			/* Use the "standard" process for client travelling to the spatial server 
			if (auto ClientConn = CreateClientConnection(SubResultMsg->connid(), ChId))
			{
				GetWorld()->GetAuthGameMode()->RestartPlayer(ClientConn->PlayerController);
			}
			*/
		}
		else
		{
			UE_LOG(LogChanneld, Log, TEXT("[Server] Sub to spatial channel %d"), ChId);
			GetChanneldSubsystem()->SetLowLevelSendToChannelId(ChId);
		}
	}
}

void USpatialChannelDataView::OnClientUnsub(ConnectionId ClientConnId, channeldpb::ChannelType ChannelType, ChannelId ChId)
{
	// A client leaves the spatial channel
	if (ChannelType == channeldpb::SPATIAL)
	{
		ClientInChannels.Remove(ClientConnId);
	}
	// A client leaves the game - close and remove the client connection.
	else if (ChannelType == channeldpb::GLOBAL)
	{
		if (auto NetDriver = GetChanneldSubsystem()->GetNetDriver())
		{
			UE_LOG(LogChanneld, Log, TEXT("Client leaves the game, removing the connection: %d"), ClientConnId);
			NetDriver->RemoveChanneldClientConnection(ClientConnId);
		}
	}
}

void USpatialChannelDataView::InitServer()
{
	// Spatial server has no authority over the GameState.
	GetWorld()->GetAuthGameMode()->GameState->SetRole(ENetRole::ROLE_SimulatedProxy);
	Super::InitServer();

	if (UClass* PlayerStartLocatorClass = GetMutableDefault<UChanneldSettings>()->PlayerStartLocatorClass)
	{
		PlayerStartLocator = NewObject<UPlayerStartLocatorBase>(this, PlayerStartLocatorClass);
	}
	ensureAlwaysMsgf(PlayerStartLocator, TEXT("[Server] SpatialChannelDataView has not specified the PlayerStartLocatorClass."));

	// Set the player's start position selected by the locator.
	FGameModeEvents::GameModePostLoginEvent.AddLambda([&](AGameModeBase* GameMode, APlayerController* NewPlayer)
	{
		auto NetConn = Cast<UChanneldNetConnection>(NewPlayer->GetNetConnection());
		AActor* StartSpot = nullptr;
		FVector StartPos = PlayerStartLocator->GetPlayerStartPosition(NetConn->GetConnId(), StartSpot);
		// Spawn a PlayerStart actor if it doesn't exist.
		if (!StartSpot)
		{
			StartSpot = GetWorld()->SpawnActor<APlayerStart>();
			StartSpot->SetActorLocation(StartPos);
		}
		NewPlayer->StartSpot = StartSpot;
		UE_LOG(LogChanneld, Log, TEXT("%s selected %s for client %d"), *PlayerStartLocator->GetName(), *StartPos.ToCompactString(), NetConn->GetConnId());
	});
	
	Connection->AddMessageHandler(channeldpb::SUB_TO_CHANNEL, this, &USpatialChannelDataView::ServerHandleSubToChannel);

	Connection->RegisterMessageHandler(unrealpb::HANDOVER_CONTEXT, new unrealpb::GetHandoverContextMessage, this, &USpatialChannelDataView::ServerHandleGetHandoverContext);
	
	Connection->AddMessageHandler(channeldpb::CHANNEL_DATA_HANDOVER, this, &USpatialChannelDataView::ServerHandleHandover);

	Connection->RegisterMessageHandler(unrealpb::SERVER_PLAYER_LEAVE, new channeldpb::ServerForwardMessage, [&](UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg)
	{
		channeldpb::UnsubscribedFromChannelResultMessage UnsubMsg;
		if (UnsubMsg.ParseFromString(static_cast<const channeldpb::ServerForwardMessage*>(Msg)->payload()))
		{
			OnClientUnsub(UnsubMsg.connid(), UnsubMsg.channeltype(), ChId);
		}
		else
		{
			UE_LOG(LogChanneld, Error, TEXT("Failed to parse the payload of the SERVER_PLAYER_LEAVE message to UnsubscribedFromChannelResultMessage. ChannelId: %d"), ChId);
		}
	});
	
	Connection->SubToChannel(GlobalChannelId, nullptr, [&](const channeldpb::SubscribedToChannelResultMessage* _)
	{
		Connection->CreateSpatialChannel(TEXT(""), nullptr, ChannelInitData ? ChannelInitData->GetMessage() : nullptr, nullptr,
			[](const channeldpb::CreateSpatialChannelsResultMessage* ResultMsg)
		{
			FString StrChIds;
			for (int i = 0; i < ResultMsg->spatialchannelid_size(); i++)
			{
				StrChIds.AppendInt(ResultMsg->spatialchannelid(i));
				if (i < ResultMsg->spatialchannelid_size() - 1)
					StrChIds.AppendChar(',');
			}
			UE_LOG(LogChanneld, Log, TEXT("Created spatial channels: %s"), *StrChIds);
		});
	});

	/*
	FGameModeEvents::GameModePostLoginEvent.AddLambda([&](AGameModeBase* GameMode, APlayerController* NewPlayer)
	{
		APawn* NewPawn = NewPlayer->GetPawn();

		unrealpb::ServerSpawnedPlayerMessage Msg;
		Msg.set_clientconnid(CastChecked<UChanneldNetConnection>(NewPlayer->GetNetConnection())->GetConnId());
		Msg.mutable_startpos()->CopyFrom(ChanneldUtils::GetVectorPB(NewPawn->GetActorLocation()));
		// Send to Master server
		Connection->Send(GlobalChannelId, unrealpb::SERVER_PLAYER_SPAWNED, Msg);
	});
	*/
}

void USpatialChannelDataView::ClientHandleSubToChannel(UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto SubResultMsg = static_cast<const channeldpb::SubscribedToChannelResultMessage*>(Msg);
	if (SubResultMsg->channeltype() == channeldpb::SPATIAL /*&& SubResultMsg->suboptions().dataaccess() == channeldpb::WRITE_ACCESS*/)
	{
		UE_LOG(LogChanneld, Log, TEXT("[Client] Sub to spatial channel %d"), ChId);
		if (bClientInMasterServer)
		{
			bClientInMasterServer = false;
			UE_LOG(LogChanneld, Log, TEXT("==================== Client no longer in Master server ===================="));
			
			GetChanneldSubsystem()->SetLowLevelSendToChannelId(ChId);

			// Join the spatial server which Master server chooses for the player.
			const FName LevelName("127.0.0.1");
			GetChanneldSubsystem()->OpenLevel(LevelName);

			// Clear the actors (PlayerController, Pawn, PlayerState, etc.) spawned when the client was in the Master server.

			if (GetMutableDefault<UChanneldSettings>()->bEnableSpatialVisualizer)
			{
				Visualizer = NewObject<USpatialVisualizer>(this, USpatialVisualizer::StaticClass());
				FTimerHandle Handle;
				GetWorld()->GetTimerManager().SetTimer(Handle, [&]()
				{
					Visualizer->Initialize(Connection);
				}, 1, false, 2.0f);
			}
		}
	}
}

void USpatialChannelDataView::ClientHandleHandover(UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto HandoverMsg = static_cast<const channeldpb::ChannelDataHandoverMessage*>(Msg);
	UE_LOG(LogChanneld, Verbose, TEXT("ChannelDataHandover from channel %d to %d"), HandoverMsg->srcchannelid(), HandoverMsg->dstchannelid());

	unrealpb::HandoverData HandoverData;
	HandoverMsg->data().UnpackTo(&HandoverData);
	for (auto& HandoverContext : HandoverData.context())
	{
		FNetworkGUID NetId(HandoverContext.obj().netguid());

		// Update NetId-ChannelId mapping
		SetOwningChannelId(NetId, HandoverMsg->dstchannelid());

		UObject* Obj = GetObjectFromNetGUID(NetId);
		if (Obj)
		{
			// Move data provider to the new channel
			if (Obj->Implements<UChannelDataProvider>())
			{
				MoveProvider(HandoverMsg->srcchannelid(), HandoverMsg->dstchannelid(), Cast<IChannelDataProvider>(Obj));
			}
			else if (Obj->IsA<AActor>())
			{
				for (auto& Comp : Cast<AActor>(Obj)->GetComponentsByInterface(UChannelDataProvider::StaticClass()))
				{
					MoveProvider(HandoverMsg->srcchannelid(), HandoverMsg->dstchannelid(), Cast<IChannelDataProvider>(Comp));
				}
			}

			if (Obj->IsA<APawn>())
			{
				APawn* Pawn = Cast<APawn>(Obj);
				if (Pawn->GetController())
				{
					// Update the channelId for LowLevelSend() if it's a player's pawn that been handed over.
					GetChanneldSubsystem()->SetLowLevelSendToChannelId(HandoverMsg->dstchannelid());

					// Also update the NetId-ChannelId mapping of the player controller (otherwise RPCs won't be processed properly)
					SetOwningChannelId(GetNetId(Pawn->GetController()), HandoverMsg->dstchannelid());
				}
			}
		}
		else
		{
			UE_LOG(LogChanneld, Warning, TEXT("Unable to find data provider to move from channel %d to %d, NetId: %d"), HandoverMsg->srcchannelid(), HandoverMsg->dstchannelid(), NetId.Value);
		}
	}
}

void USpatialChannelDataView::InitClient()
{
	Super::InitClient();

	Connection->AddMessageHandler(channeldpb::SUB_TO_CHANNEL, this, &USpatialChannelDataView::ClientHandleSubToChannel);
	Connection->AddMessageHandler(channeldpb::CHANNEL_DATA_HANDOVER, this, &USpatialChannelDataView::ClientHandleHandover);
	
	channeldpb::ChannelSubscriptionOptions GlobalSubOptions;
	GlobalSubOptions.set_dataaccess(channeldpb::READ_ACCESS);
	GlobalSubOptions.set_fanoutintervalms(20);
	GlobalSubOptions.set_fanoutdelayms(1000);
	Connection->SubToChannel(GlobalChannelId, &GlobalSubOptions, [&](const channeldpb::SubscribedToChannelResultMessage* Msg)
	{
		// GetChanneldSubsystem()->SetLowLevelSendToChannelId(GlobalChannelId);
		// // Join Master server as soon as the client subscribed.
		// GetChanneldSubsystem()->OpenLevel(FName("127.0.0.1"));
		bClientInMasterServer = true;
		UE_LOG(LogChanneld, Log, TEXT("==================== Client starts in Master server ===================="));
	});
}

bool USpatialChannelDataView::GetSendToChannelId(UChanneldNetConnection* NetConn, uint32& OutChId) const
{
	const ChannelId* ChId = ClientInChannels.Find(NetConn->GetConnId());
	if (ChId)
	{
		OutChId = *ChId;
		return true;
	}
	return false;
}

void USpatialChannelDataView::AddProvider(ChannelId ChId, IChannelDataProvider* Provider)
{
	/*
	bool bIsGameStateBase = Provider->GetTargetObject()->IsA(AGameStateBase::StaticClass());
	// Don't perform replication (except the GameStateBase) when the client is in Master server
	if (bClientInMasterServer && !bIsGameStateBase)
	{
		UE_LOG(LogChanneld, Log, TEXT("[Client] Ignore adding provider %s while in Master server"), *IChannelDataProvider::GetName(Provider));
		return;
	}

	// GameStateBase is owned by Master server and replicated in Global channel. Read only to spatial servers.
	if (bIsGameStateBase)
	{
		ChId = GlobalChannelId;
		AGameStateBase* GameStateBase = Cast<AGameStateBase>(Provider->GetTargetObject());
		GameStateBase->SetRole(ENetRole::ROLE_SimulatedProxy);
	}
	*/
	
	Super::AddProvider(ChId, Provider);
}

void USpatialChannelDataView::AddProviderToDefaultChannel(IChannelDataProvider* Provider)
{
	if (Connection->IsServer())
	{
		FNetworkGUID NetId = GetNetId(Provider);
		if (NetIdOwningChannels.Contains(NetId))
		{
			Super::AddProviderToDefaultChannel(Provider);
		}
		else if (const AActor* Actor = Cast<AActor>(Provider->GetTargetObject()))
		{
			if (Actor->IsA<AInfo>())
			{
				UE_LOG(LogChanneld, Verbose, TEXT("Ignore querying AInfo %s's spatial channel"), *Actor->GetName());
				return;
			}
			TArray<FVector> Positions;
			Positions.Add(Actor->GetActorLocation());
			Connection->QuerySpatialChannel(Positions, [&, NetId, Provider, Actor](const channeldpb::QuerySpatialChannelResultMessage* ResultMsg)
			{
				const ChannelId SpatialChId = ResultMsg->channelid(0);
				UE_LOG(LogChanneld, Log, TEXT("Queried spatial channelId %d for provider: %s"), SpatialChId, *IChannelDataProvider::GetName(Provider));
				SetOwningChannelId(NetId, SpatialChId);
				AddProvider(SpatialChId, Provider);
				// Add the PlayerController and the PlayerState to the same spatial channel as the Pawn.
				if (Actor->IsA<APawn>())
				{
					const APawn* Pawn = Cast<APawn>(Actor);
					AddActorProvider(SpatialChId, Pawn->GetController());
					AddActorProvider(SpatialChId, Pawn->GetPlayerState());
				}
				
				//SendSpawnToAdjacentChannels(Provider->GetTargetObject(), SpatialChId);
			});
		}
		else
		{
			UE_LOG(LogChanneld, Warning, TEXT("Failed to add provider: can't retrieve location from provider: %s"), *IChannelDataProvider::GetName(Provider));
		}
	}
	else
	{
		Super::AddProviderToDefaultChannel(Provider);
	}
}

void USpatialChannelDataView::RemoveProvider(ChannelId ChId, IChannelDataProvider* Provider, bool bSendRemoved)
{
	// // Don't perform replication when the client is in Master server
	// if (bClientInMasterServer)
	// 	return;
	
	Super::RemoveProvider(ChId, Provider, bSendRemoved);
}

void USpatialChannelDataView::OnAddClientConnection(UChanneldNetConnection* ClientConnection, ChannelId ChId)
{
	ClientInChannels.Add(ClientConnection->GetConnId(), ChId);
}

void USpatialChannelDataView::OnRemoveClientConnection(UChanneldNetConnection* ClientConn)
{
	ClientInChannels.Remove(ClientConn->GetConnId());
}

void USpatialChannelDataView::OnClientPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer, UChanneldNetConnection* NewPlayerConn)
{
	Super::OnClientPostLogin(GameMode, NewPlayer, NewPlayerConn);

	/* Copied from Super::OnClientPostLogin - prevent from sending GameStateBase to the new player.
	// Send the existing player pawns to the new player
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PC = Iterator->Get();
		if (PC && PC != NewPlayer && PC->GetPawn())
		{
			NewPlayerConn->SendSpawnMessage(PC->GetPawn(), ENetRole::ROLE_SimulatedProxy);
		}
	}
	*/
}

bool USpatialChannelDataView::OnServerSpawnedObject(UObject* Obj, const FNetworkGUID NetId)
{
	// GameState is in GLOBAL channel
	if (Obj->IsA<AGameStateBase>())
	{
		SetOwningChannelId(NetId, GlobalChannelId);
	}
	
	if (!bSuppressAddProviderOnServerSpawn)
	{
		// Don't set the NetId-ChannelId mapping, as we don't have the spatial channelId of the object yet.
		// The spatial channelId will be queried in AddProviderToDefaultChannel()
		if (Obj->IsA<AActor>())
		{
			AddActorProvider(Cast<AActor>(Obj));
		}
	}
	return true;
}

void USpatialChannelDataView::SendSpawnToConn(UObject* Obj, UChanneldNetConnection* NetConn, uint32 OwningConnId)
{
	// GameState is spawned via GLOBAL channel
	if (Obj->IsA<AGameStateBase>())
	{
		NetConn->SendSpawnMessage(Obj, ROLE_SimulatedProxy, GlobalChannelId);
	}
	else if (AActor* Actor = Cast<AActor>(Obj))
	{
		// Location must be set for channeld to update the spatial channelId.
		FVector Location = Actor->GetActorLocation();
		// Set the location of PlayerController or PlayerState to the start position, to make sure them are in the same spatial channel as the pawn.
		if (Actor->IsA<APlayerController>() || Actor->IsA<APlayerState>())
		{
			AActor* StartSpot;
			Location = PlayerStartLocator->GetPlayerStartPosition(OwningConnId, StartSpot);
			Actor->SetActorLocation(Location);
		}
		NetConn->SendSpawnMessage(Actor, Actor->IsA<APlayerController>() ? ROLE_AutonomousProxy : Actor->GetRemoteRole(), GetChanneldSubsystem()->LowLevelSendToChannelId.Get(), OwningConnId, &Location);
	}
	else
	{
		Super::SendSpawnToConn(Obj, NetConn, OwningConnId);
	}
}

void USpatialChannelDataView::SendSpawnToAdjacentChannels(UObject* Obj, ChannelId SpatialChId)
{
	auto NetDriver = GetChanneldSubsystem()->GetNetDriver();
	if (!NetDriver)
	{
		UE_LOG(LogChanneld, Error, TEXT("USpatialChannelDataView::SendSpawnToAdjacentChannels: Unable to get ChanneldNetDriver"));
		return;
	}
	
	const FNetworkGUID NetId = NetDriver->GuidCache->GetOrAssignNetGUID(Obj);
	
	unrealpb::SpawnObjectMessage SpawnMsg;
	SpawnMsg.mutable_obj()->CopyFrom(ChanneldUtils::GetRefOfObject(Obj));
	if (Obj->IsA<AActor>())
	{
		AActor* Actor = Cast<AActor>(Obj);
		SpawnMsg.set_localrole(Actor->GetRemoteRole());
		if (auto NetConn = Cast<UChanneldNetConnection>(Actor->GetNetConnection()))
		{ 
			SpawnMsg.set_owningconnid(NetConn->GetConnId());
		}
		// Also set the spatial info for channeld to update the spatial channelId
		SpawnMsg.mutable_location()->MergeFrom(ChanneldUtils::GetVectorPB(Actor->GetActorLocation()));
	}
	SpawnMsg.set_channelid(SpatialChId);
	Connection->Broadcast(SpatialChId, unrealpb::SPAWN, SpawnMsg, channeldpb::ADJACENT_CHANNELS | channeldpb::ALL_BUT_SENDER);
	UE_LOG(LogChanneld, Log, TEXT("[Server] Broadcasted Spawn message to spatial channels(%d), obj: %s, netId: %d"), SpatialChId, *Obj->GetName(), NetId.Value);

	NetDriver->SetAllSentSpawn(NetId);
}
