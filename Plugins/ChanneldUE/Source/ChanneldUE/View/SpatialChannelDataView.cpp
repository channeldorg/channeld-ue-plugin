// Fill out your copyright notice in the Description page of Project Settings.


#include "View/SpatialChannelDataView.h"

#include "ChanneldGameInstanceSubsystem.h"
#include "ChanneldNetConnection.h"
#include "ChanneldNetDriver.h"
#include "ChanneldUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
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
		//GameMode->ChangeName(NewPlayerController, FString::Printf(TEXT("Player_%d"), ClientConn->GetConnId()), false);
		// ~ End copy of AGameModeBase::InitNewPlayer
		
		// ~ End copy of AGameModeBase::Login
		
		// Possess the newly-spawned player.
		NewPlayerController->NetPlayerIndex = 0;

		// HACK: We need to set the private property bIsLocalPlayerController to false before calling SetPlayer(), so it won't create the spectator pawn for the PC.
		static const FBoolProperty* Prop = CastFieldChecked<const FBoolProperty>(APlayerController::StaticClass()->FindPropertyByName("bIsLocalPlayerController"));
		/* Won't work - causes error
		Prop->SetPropertyValue(NewPlayerController, false);
		*/
		bool* Ptr = Prop->ContainerPtrToValuePtr<bool>(NewPlayerController);
		*Ptr = false;
		UE_LOG(LogChanneld, VeryVerbose, TEXT("Set bIsLocalPlayerController to %s"), NewPlayerController->IsLocalController() ? TEXT("true") : TEXT("false"));
		
		NewPlayerController->SetPlayer(ClientConn);

		// IMPORTANT: Set ROLE_Authority must be done after SetPlayer()!
		NewPlayerController->SetRole(ROLE_Authority);
		NewPlayerController->SetReplicates(true);

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

TArray<UObject*> USpatialChannelDataView::GetHandoverObjects(UObject* Obj, ChannelId SrcChId, ChannelId DstChId)
{
	TArray<UObject*> Result;
	Result.Add(Obj);
	
	// Group Pawn, PlayerController, and PlayerState together to make them handover at the same time.
	if (const APawn* Pawn = Cast<APawn>(Obj))
	{
		Result.Add(Pawn->GetController());
		Result.Add(Pawn->GetPlayerState());
	}

	return Result;
}

void USpatialChannelDataView::ServerHandleGetHandoverContext(UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto GetContextMsg = static_cast<const unrealpb::GetHandoverContextMessage*>(Msg);
	const auto NetId = FNetworkGUID(GetContextMsg->netid());
	UObject* Obj = GetObjectFromNetGUID(NetId);

	unrealpb::GetHandoverContextResultMessage ResultMsg;
	ResultMsg.set_netid(NetId.Value);
	ResultMsg.set_srcchannelid(GetContextMsg->srcchannelid());
	ResultMsg.set_dstchannelid(GetContextMsg->dstchannelid());

	if (Obj == nullptr)
	{
		UE_LOG(LogChanneld, Error, TEXT("Unable to get handover context, netId: %d"), NetId.Value);
		// Always send back the result message.
		Connection->Send(ChId, unrealpb::HANDOVER_CONTEXT, ResultMsg);
		return;
	}

	TArray<UObject*> HandoverObjs = GetHandoverObjects(Obj, GetContextMsg->srcchannelid(), GetContextMsg->dstchannelid());

	for (UObject* HandoverObj : HandoverObjs)
	{
		if (HandoverObj == nullptr)
		{
			continue;
		}
		auto HandoverContext = ResultMsg.add_context();
		UChanneldNetConnection* NetConn = nullptr;
		if (AActor* HandoverActor = Cast<AActor>(HandoverObj))
		{
			NetConn = Cast<UChanneldNetConnection>(HandoverActor->GetNetConnection());
		}
		HandoverContext->mutable_obj()->CopyFrom(ChanneldUtils::GetRefOfObject(HandoverObj, NetConn));
		if (NetConn)
		{
			HandoverContext->set_clientconnid(NetConn->GetConnId());
		}
	}

	/*
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
	*/

	UE_LOG(LogChanneld, Verbose, TEXT("[Server] GetHandoverContext for channeld: %s"), UTF8_TO_TCHAR(ResultMsg.DebugString().c_str()));
	
	Connection->Send(ChId, unrealpb::HANDOVER_CONTEXT, ResultMsg);
}

void USpatialChannelDataView::ServerHandleHandover(UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto HandoverMsg = static_cast<const channeldpb::ChannelDataHandoverMessage*>(Msg);
	unrealpb::HandoverData HandoverData;
	HandoverMsg->data().UnpackTo(&HandoverData);

	// Does current server has interest over the handover objects?
	const bool bHasInterest = Connection->SubscribedChannels.Contains(HandoverMsg->dstchannelid());
	// Does current server has authority over the handover objects?
	const bool bHasAuthority = Connection->OwnedChannels.Contains(HandoverMsg->dstchannelid());
	
	FString NetIds;
	for (auto& HandoverContext : HandoverData.context())
	{
		NetIds.Appendf(TEXT("%d[%d], "), HandoverContext.obj().netguid(), HandoverContext.clientconnid());
	}
	UE_LOG(LogChanneld, Log, TEXT("ChannelDataHandover from channel %d to %d(%s), object netIds: %s"), HandoverMsg->srcchannelid(), HandoverMsg->dstchannelid(),
		bHasAuthority ? TEXT("A") : (bHasInterest ? TEXT("I") : TEXT("")), *NetIds);

	// The actors are handed over to current (destination) server.
	TArray<AActor*> CrossServerActors;
	
	for (auto& HandoverContext : HandoverData.context())
	{
		const unrealpb::UnrealObjectRef HandoverObjRef = HandoverContext.obj();
		FNetworkGUID NetId(HandoverObjRef.netguid());
		
		// Set the NetId-ChannelId mapping before spawn the object, so AddProviderToDefaultChannel won't have to query the spatial channel.
		SetOwningChannelId(NetId, HandoverMsg->dstchannelid());
	
		// Source spatial server - the channel data is handed over from
		if (Connection->SubscribedChannels.Contains(HandoverMsg->srcchannelid()))
		{
			UObject* HandoverObj = GetObjectFromNetGUID(NetId);
			// Check if object is already destroyed
			if (IsValid(HandoverObj))
			{
				// If the handover actor is no longer in the interest area of current server, delete it.
				if (!bHasInterest)
				{
					UE_LOG(LogChanneld, Log, TEXT("[Server] Deleting object %s as it leaves the interest area"), *HandoverObj->GetName());
					if (AActor* HandoverActor = Cast<AActor>(HandoverObj))
					{
						if (auto HandoverPC = Cast<APlayerController>(HandoverActor))
						{
							HandoverPC->Player = nullptr;
							
							// Don't remove the client connection for now - we don't want to send CloseBunch to the client.
							// Keep the client connection until the client leaves the game, so we can reuse it for handover.
							HandoverPC->NetConnection = nullptr;
							
							// Make sure the PlayerState is not destroyed together - we will handle it in the following handover context loop
							HandoverPC->PlayerState = nullptr;
						}
						else if (auto HandoverPawn = Cast<APawn>(HandoverActor))
						{
							// Reset the pawn's controller so it won't send ClientSetViewTarget RPC in APawn::DetachFromControllerPendingDestroy
							HandoverPawn->Controller = nullptr;
						}
						
						// Don't send "removed: true" update to the channel, because the state still exists.
						RemoveActorProvider(HandoverActor, false);
						
						GetWorld()->DestroyActor(HandoverActor, true);
					}
					else
					{
						HandoverObj->ConditionalBeginDestroy();
					}

					/* Don't remove the mapping, as it may be used for getting the target channel of unresolvable RPC
					NetIdOwningChannels.Remove(NetId);
					*/

					// Remove from the NetGUIDCache
					auto GuidCache = GetWorld()->NetDriver->GuidCache;
					GuidCache->ObjectLookup.Remove(NetId);
					GuidCache->NetGUIDLookup.Remove(HandoverObj);
				}
				// If the handover actor is no longer in the authority area of current server,
				// make sure it no longer sends ChannelDataUpdate message.
				else if (!bHasAuthority)
				{
					if (AActor* HandoverActor = Cast<AActor>(HandoverObj))
					{
						HandoverActor->SetRole(ENetRole::ROLE_SimulatedProxy);
					}
				}
			}
		}
		
		
		// Destination spatial server - the channel data is handed over to
		if (bHasInterest)
		{
			UObject* HandoverObj = GetObjectFromNetGUID(NetId);
			// We need to know which client connection causes the handover, in order to spawn the object. 
			UChanneldNetConnection* ClientConn = nullptr;

			// Player enters the server - only Pawn, PlayerController or PlayerState has clientConnId > 0
			if (HandoverContext.clientconnid() > 0)
			{
				const ConnectionId ClientConnId = HandoverContext.clientconnid();
				ClientConn = GetChanneldSubsystem()->GetNetDriver()->GetClientConnection(ClientConnId);
			
				// Try to spawn the object
				if (HandoverObj == nullptr)
				{
					if (ClientInChannels.Contains(ClientConnId))
					{
						// Update the channelId for LowLevelSend()
						ClientInChannels.Emplace(ClientConnId, HandoverMsg->dstchannelid());
						UE_LOG(LogChanneld, Log, TEXT("[Server] Updated mapping of connId: %d -> channelId: %d"), ClientConnId, HandoverMsg->dstchannelid());
					}
					else
					{
						// Create the client connection if it doesn't exist yet. Don't create the PlayerController for now.
						ClientConn = CreateClientConnection(ClientConnId, HandoverMsg->dstchannelid());
					}

				
					// Set the NetId of the handover object as exported, so the NetConn won't send the Spawn message to the client.
					// FIXME: Won't work as the spawned object will have a different NetId!
					ClientConn->SetSentSpawned(NetId);

					// HACK: turn off AddProviderToDefaultChannel() temporarily, as the NetId of the handover object may mismatch when just created. (ChanneldUtils@L656)
					bSuppressAddProviderAndSendOnServerSpawn = true;
					HandoverObj = ChanneldUtils::GetObjectByRef(&HandoverObjRef, GetWorld(), true, ClientConn);
					bSuppressAddProviderAndSendOnServerSpawn = false;
					UE_LOG(LogChanneld, Log, TEXT("[Server] Spawned handover obj: %s, clientConnId: %d"), *GetNameSafe(HandoverObj), ClientConnId);
				}
			}
			// Non-player object enters the server. Simply create it.
			else
			{
				HandoverObj = ChanneldUtils::GetObjectByRef(&HandoverObjRef, GetWorld(), true);
			}
		
			if (IsValid(HandoverObj))
			{
				// In-server handover - the srcChannel and dstChannel are in the same server
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
				// Cross-server handover
				else
				{
					// Now the NetId is properly set, call AddProviderToDefaultChannel().
					OnServerSpawnedObject(HandoverObj, NetId);

					if (AActor* HandoverActor = Cast<AActor>(HandoverObj))
					{
						CrossServerActors.Add(HandoverActor);

						// Associate the NetConn with the PlayerController
						if (APlayerController* HandoverPC = Cast<APlayerController>(HandoverObj))
						{
							HandoverPC->NetConnection = ClientConn;
						}
						
						// Set the role to SimulatedProxy so the actor can be updated by the handover channel data later.
						HandoverActor->SetRole(ROLE_SimulatedProxy);
						UE_LOG(LogChanneld, Verbose, TEXT("Set %s to ROLE_SimulatedProxy for ChannelDataUpdate"), *HandoverActor->GetName());
					}
				}
			}
			else if (HandoverObj == nullptr)
			{
				UE_LOG(LogChanneld, Warning, TEXT("[Server] Failed to spawn object from handover data: %s"), UTF8_TO_TCHAR(HandoverData.DebugString().c_str()));
			}
			else
			{
				UE_LOG(LogChanneld, Warning, TEXT("[Server] Handover object '%s' is not valid, pending kill: %d"), *GetNameSafe(HandoverObj), HandoverObj->IsPendingKill());
			}
		}
	}

	// Applies the channel data update to the newly spawned objects.
	// The references between the Pawn, PlayerController, and PlayerState should be set properly in this step.
	if (CrossServerActors.Num() > 0 && HandoverData.has_channeldata())
	{
		channeldpb::ChannelDataUpdateMessage UpdateMsg;
		/* DO NOT use set_allocated_data - the HandoverData is in the stack memory but set_allocated_data requires the data in the heap.
		 * The following line will cause the Replicator's FullState merges the in-stack Arena which will be deallocated later,
		 * then the Replicator's next Tick() would trigger "Access Violation" exception.
		UpdateMsg.set_allocated_data(HandoverData.mutable_channeldata());
		*/
		UpdateMsg.mutable_data()->CopyFrom(HandoverData.channeldata());
		HandleChannelDataUpdate(_, ChId, &UpdateMsg);
	}

	// Post handover - set the actors' properties as same as they were in the source server.
	
	// Pass 1
	for (auto HandoverActor : CrossServerActors)
	{
		if (auto HandoverPC = Cast<APlayerController>(HandoverActor))
		{
			// ensureAlwaysMsgf(HandoverPC->GetPawn() != nullptr, TEXT("PlayerController doesn't have Pawn set properly"));
			// ensureAlwaysMsgf(HandoverPC->PlayerState != nullptr, TEXT("PlayerController doesn't have PlayerState set properly"));
			
			if (auto ClientConn = Cast<UChanneldNetConnection>(HandoverPC->NetConnection))
			{
				// Set the role to SimulatedProxy so it won't unpossess the pawn in BeginSpectatingState()
				HandoverPC->SetRole(ROLE_SimulatedProxy);
				// Call this after HandleChannelDataUpdate(), as RegisterPlayer() requires the PC has PlayerState set.
				InitPlayerController(ClientConn, HandoverPC);
				UE_LOG(LogChanneld, Verbose, TEXT("[Server] Initialized PlayerController with client conn %d"), ClientConn->GetConnId());
			}
			else
			{
				UE_LOG(LogChanneld, Error, TEXT("[Server] Unable to associate PlayerController with any NetConnection."));
			}			
			
			HandoverPC->ServerAcknowledgePossession(HandoverPC->GetPawn());

			ensureAlwaysMsgf(HandoverPC->GetPawn() && HandoverPC->GetPawn()->GetNetConnection(),
				TEXT("Handover Pawn '%s' should have NetConn set!"), *GetNameSafe(HandoverPC->GetPawn()));
		}
	}

	// Pass 2
	for (auto HandoverActor : CrossServerActors)
	{
		// If the handover actor falls into the authority area of current server,
		// make sure it starts sending ChannelDataUpdate message. It has to be done AFTER calling HandleChannelDataUpdate.
		if (Connection->OwnedChannels.Contains(HandoverMsg->dstchannelid()))
		{
			// CAUTION: Setting to ROLE_Authority should be performed at last, as many UE code assume it's in the client.
			HandoverActor->SetRole(ENetRole::ROLE_Authority);
			UE_LOG(LogChanneld, Verbose, TEXT("Set %s to back to ROLE_Authority after ChannelDataUpdate"), *HandoverActor->GetName());
		}
	}
}

void USpatialChannelDataView::ServerHandleSubToChannel(UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg)
{
	const auto SubResultMsg = static_cast<const channeldpb::SubscribedToChannelResultMessage*>(Msg);
	UE_LOG(LogChanneld, Log, TEXT("[Server] Sub %s conn %d to %s channel %d"),
		SubResultMsg->conntype() == channeldpb::CLIENT ? TEXT("client") : TEXT("server"),
		SubResultMsg->connid(),
		UTF8_TO_TCHAR(channeldpb::ChannelType_Name(SubResultMsg->channeltype()).c_str()),
		ChId);
	
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
	UE_LOG(LogChanneld, Log, TEXT("[Client] Sub to %s channel %d"), UTF8_TO_TCHAR(channeldpb::ChannelType_Name(SubResultMsg->channeltype()).c_str()), ChId);
	if (SubResultMsg->channeltype() == channeldpb::SPATIAL /*&& SubResultMsg->suboptions().dataaccess() == channeldpb::WRITE_ACCESS*/)
	{
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
				Visualizer->Initialize(Connection);
			}
		}
	}
}

void USpatialChannelDataView::ClientHandleHandover(UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg)
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
				// Only the client's owning Pawn has the PlayerController
				if (Pawn->GetController())
				{
					// Reset the movement data to avoid timestamp discrepancy on the dest server.
					if (ACharacter* Char = Cast<ACharacter>(Pawn))
					{
						// has_channeldata = true means the handover is between spatial servers.
						if (HandoverData.has_channeldata() && Char->GetCharacterMovement() != nullptr)
						{
							// Char->GetCharacterMovement()->StopMovementImmediately();
							Char->GetCharacterMovement()->ResetPredictionData_Client();
							UE_LOG(LogChanneld, Verbose, TEXT("Reset client's movement prediction data."));
						}
					}
					
					// Update the channelId for LowLevelSend() if it's a player's pawn that been handed over.
					GetChanneldSubsystem()->SetLowLevelSendToChannelId(HandoverMsg->dstchannelid());

					// Also update the NetId-ChannelId mapping of the player controller (otherwise RPCs won't be processed properly)
					SetOwningChannelId(GetNetId(Pawn->GetController()), HandoverMsg->dstchannelid());
				}
			}
		}
		else
		{
			// FIXME: We can't wait the server to send the Spawn messages as it's suppressed in the handover process.
			// But we also don't want to spawn the PlayerState or PlayerController of other players.
			
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
	GlobalSubOptions.set_fanoutintervalms(50);
	// Make the first fan-out of GLOBAL channel data update (GameStateBase) a bit slower, so the GameState is spawned from the spatial server and ready.
	GlobalSubOptions.set_fanoutdelayms(2000);
	Connection->SubToChannel(GlobalChannelId, &GlobalSubOptions, [&](const channeldpb::SubscribedToChannelResultMessage* Msg)
	{
		// GetChanneldSubsystem()->SetLowLevelSendToChannelId(GlobalChannelId);
		// // Join Master server as soon as the client subscribed.
		// GetChanneldSubsystem()->OpenLevel(FName("127.0.0.1"));
		bClientInMasterServer = true;
		UE_LOG(LogChanneld, Log, TEXT("==================== Client starts in Master server ===================="));
	});
}

void USpatialChannelDataView::SetOwningChannelId(const FNetworkGUID NetId, ChannelId ChId)
{
	Super::SetOwningChannelId(NetId, ChId);

	if (Visualizer)
	{
		Visualizer->OnUpdateOwningChannel(GetObjectFromNetGUID(NetId), ChId);
	}
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

void USpatialChannelDataView::OnClientSpawnedObject(UObject* Obj, const ChannelId ChId)
{
	if (Visualizer)
	{
		Visualizer->OnSpawnedObject(Obj, ChId);
	}
}

bool USpatialChannelDataView::OnServerSpawnedObject(UObject* Obj, const FNetworkGUID NetId)
{
	// GameState is in GLOBAL channel
	if (Obj->IsA<AGameStateBase>())
	{
		SetOwningChannelId(NetId, GlobalChannelId);
	}

	if (bSuppressAddProviderAndSendOnServerSpawn)
	{
		return false;
	}
	
	// Don't set the NetId-ChannelId mapping, as we don't have the spatial channelId of the object yet.
	// The spatial channelId will be queried in AddProviderToDefaultChannel()
	if (Obj->IsA<AActor>())
	{
		AddActorProvider(Cast<AActor>(Obj));
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
