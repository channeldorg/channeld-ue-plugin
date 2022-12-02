// Fill out your copyright notice in the Description page of Project Settings.


#include "View/SpatialChannelDataView.h"

#include "ChanneldGameInstanceSubsystem.h"
#include "ChanneldNetConnection.h"
#include "ChanneldNetDriver.h"
#include "ChanneldUtils.h"
#include "Net/DataChannel.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerStart.h"

USpatialChannelDataView::USpatialChannelDataView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UChanneldNetConnection* USpatialChannelDataView::CreateClientConnection(ConnectionId ConnId, ChannelId ChId)
{
	ensureMsgf(!ClientInChannels.Contains(ConnId), TEXT("Client conn %d had already been added to this server"), ConnId);

	if (auto NetDriver = GetChanneldSubsystem()->GetNetDriver())//NetDriver.IsValid())
	{
		UChanneldNetConnection* ClientConn = NetDriver->AddChanneldClientConnection(ConnId);
		// Create the ControlChannel and set OpenAcked = 1
		UChannel* ControlChannel = ClientConn->CreateChannelByName(NAME_Control, EChannelCreateFlags::OpenedLocally);
		ControlChannel->OpenAcked = 1;
		ControlChannel->OpenPacketId.First = 0;
		ControlChannel->OpenPacketId.Last = 0;

		//~ Begin copy of UWorld::NotifyControlMessage
		FURL InURL( NULL, *ClientConn->RequestURL, TRAVEL_Absolute );
		FString ErrorMsg;
		ClientConn->PlayerController = GetWorld()->SpawnPlayActor( ClientConn, ROLE_AutonomousProxy, InURL, ClientConn->PlayerId, ErrorMsg );
		ClientConn->SetClientLoginState(EClientLoginState::ReceivedJoin);
		//~ End copy
		
		ClientInChannels.Add(ConnId, ChId);

		return ClientConn;
	}
	else
	{
		UE_LOG(LogChanneld, Error, TEXT("Spatial server failed to create client connection %d: NetDriver is not valid."), ConnId);
	}
	
	return nullptr;
}

void USpatialChannelDataView::ServerHandleHandover(UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto HandoverMsg = static_cast<const channeldpb::ChannelDataHandoverMessage*>(Msg);
	unrealpb::HandoverData HandoverData;
	HandoverMsg->data().UnpackTo(&HandoverData);
	const unrealpb::UnrealObjectRef HandoverObjRef = HandoverData.obj();
	FNetworkGUID NetId(HandoverObjRef.netguid());
	UE_LOG(LogChanneld, Verbose, TEXT("ChannelDataHandover from channel %d to %d, object NetId: %d"), HandoverMsg->srcchannelid(), HandoverMsg->dstchannelid(), NetId.Value);
	
	// Source spatial server - the channel data is handed over from
	if (Connection->SubscribedChannels.Contains(HandoverMsg->srcchannelid()))
	{
		AActor* HandoverActor = Cast<AActor>(GetObjectFromNetGUID(NetId));
			
		// If the handover actor is no longer in the interest area of current server, delete it.
		if (!Connection->SubscribedChannels.Contains(HandoverMsg->dstchannelid()))
		{
			if (HandoverActor)
			{
				GetWorld()->DestroyActor(HandoverActor, true);
				// TODO: Remove the client connection if the object represents a player
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

		/*
		// Update the client's data access in the srcChannel to READ
		// FIXME: should we put this logic in channeld?
		channeldpb::ChannelSubscriptionOptions NonAuthSubOptions;
		NonAuthSubOptions.set_dataaccess(channeldpb::READ_ACCESS);
		Connection->SubConnectionToChannel(HandoverMsg->contextconnid(), HandoverMsg->srcchannelid(), &NonAuthSubOptions);
		*/
	}
		
		
	// Destination spatial server - the channel data is handed over to
	if (Connection->SubscribedChannels.Contains(HandoverMsg->dstchannelid()))
	{
		// Set the NetId-ChannelId mapping before spawn the object.
		SetOwningChannelId(NetId, HandoverMsg->dstchannelid());
		
		UObject* HandoverObj = nullptr;
		if (HandoverData.clientconnid() > 0)
		{
			UChanneldNetConnection* ClientConn;
			if (ClientInChannels.Contains(HandoverData.clientconnid()))
			{
				ClientConn = GetChanneldSubsystem()->GetNetDriver()->GetClientConnection(HandoverData.clientconnid());
			}
			else
			{
				// Create the client connection if it doesn't exist yet
				ClientConn = CreateClientConnection(HandoverData.clientconnid(), HandoverMsg->dstchannelid());
			}
			// Update the channelId for LowLevelSend()
			ClientInChannels[HandoverData.clientconnid()] = HandoverMsg->dstchannelid();
			UE_LOG(LogChanneld, Log, TEXT("[Server] Updated mapping of connId: %d -> channelId: %d"), HandoverMsg->contextconnid(), HandoverMsg->dstchannelid());

			HandoverObj = ChanneldUtils::GetObjectByRef(&HandoverObjRef, GetWorld(), true, ClientConn);
			
			if (HandoverObj && HandoverObj->IsA<APawn>())
			{
				ClientConn->PlayerController->Possess(Cast<APawn>(HandoverObj));
			}
		}
		else
		{
			HandoverObj = ChanneldUtils::GetObjectByRef(&HandoverObjRef, GetWorld(), true);
		}
		
		if (HandoverObj)
		{
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

		/*
		
		// Set the NetId-ChannelId mapping before spawn the object.
		SetOwningChannelId(NetId, HandoverMsg->dstchannelid());
			
		// Spawn the handover object if it doesn't exist before
		UObject* HandoverObj = ChanneldUtils::GetObjectByRef(&HandoverObjRef, GetWorld(), true);
			
		if (!Connection->SubscribedChannels.Contains(HandoverMsg->srcchannelid()))
		{
			if (HandoverMsg->contextconnid() > 0)
			{
				// Update the channelId for LowLevelSend()
				ClientInChannels[HandoverMsg->contextconnid()] = HandoverMsg->dstchannelid();
				UE_LOG(LogChanneld, Log, TEXT("[Server] Updated mapping of connId: %d -> channelId: %d"), HandoverMsg->contextconnid(), HandoverMsg->dstchannelid());
			}
				
			if (HandoverObj)
			{
				// Create the client connection and player controller for the player
				if (HandoverObj->IsA<APawn>())
				{
					if (auto ClientConn = CreateClientConnection(HandoverMsg->contextconnid(), HandoverMsg->dstchannelid()))
					{
						ClientConn->PlayerController->Possess(Cast<APawn>(HandoverObj));
					}
				}
			}
			else
			{
				UE_LOG(LogChanneld, Warning, TEXT("[Server] Failed to spawn object from handover, ObjRef: %s"), UTF8_TO_TCHAR(HandoverObjRef.DebugString().c_str()));
			}
		}
		// The handover happened within the server - src and dst channels are both subscribed.
		else
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

		*/

		// If the handover actor falls into the authority area of current server,
		// make sure it starts sending ChannelDataUpdate message.
		if (Connection->OwnedChannels.Contains(HandoverMsg->dstchannelid()))
		{
			if (HandoverObj && HandoverObj->IsA<AActor>())
			{
				Cast<AActor>(HandoverObj)->SetRole(ENetRole::ROLE_Authority);
			}
		}

		
		/*
		// Switch the client's data acess in the dstChannel to WRITE
		// FIXME: should we put this logic in channeld?
		channeldpb::ChannelSubscriptionOptions AuthSubOptions;
		AuthSubOptions.set_dataaccess(channeldpb::WRITE_ACCESS);
		Connection->SubConnectionToChannel(HandoverMsg->contextconnid(), HandoverMsg->dstchannelid(), &AuthSubOptions);
		*/
	}
}

void USpatialChannelDataView::InitServer()
{
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
	
	Connection->AddMessageHandler(channeldpb::SUB_TO_CHANNEL, [&](UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg)
	{
		auto SubResultMsg = static_cast<const channeldpb::SubscribedToChannelResultMessage*>(Msg);
        // A client is subscribed to a spatial channel the server owns (Sub message is sent by Master server)
		if (SubResultMsg->channeltype() == channeldpb::SPATIAL /*&& SubResultMsg->suboptions().dataaccess() == channeldpb::WRITE_ACCESS*/)
		{
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
		
	});
	
	Connection->AddMessageHandler(channeldpb::UNSUB_FROM_CHANNEL, [&](UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg)
	{
		auto UnsubResultMsg = static_cast<const channeldpb::UnsubscribedFromChannelResultMessage*>(Msg);
		// A client unsubscribed from the spatial channel
		if (UnsubResultMsg->conntype() == channeldpb::CLIENT && UnsubResultMsg->channeltype() == channeldpb::SPATIAL)
		{
			// UChanneldNetDriver::ServerHandleUnsub will handle the player disconnection.
			
			ClientInChannels.Remove(UnsubResultMsg->connid());
		}
	});

	Connection->AddMessageHandler(channeldpb::CHANNEL_DATA_HANDOVER, this, &USpatialChannelDataView::ServerHandleHandover);

	
	Connection->SubToChannel(GlobalChannelId, nullptr, [&](const channeldpb::SubscribedToChannelResultMessage* _)
	{
		Connection->CreateSpatialChannel(TEXT(""), nullptr, nullptr, nullptr, [](const channeldpb::CreateSpatialChannelsResultMessage* ResultMsg)
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
		Connection->Send(GlobalChannelId, MessageType_SERVER_PLAYER_SPAWNED, Msg);
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
	FNetworkGUID NetId(HandoverData.obj().netguid());

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
			TArray<FVector> Positions;
			Positions.Add(Actor->GetActorLocation());
			Connection->QuerySpatialChannel(Positions, [&, NetId, Provider](const channeldpb::QuerySpatialChannelResultMessage* ResultMsg)
			{
				const ChannelId SpatialChId = ResultMsg->channelid(0);
				UE_LOG(LogChanneld, Log, TEXT("Queried spatial channelId %d for provider: %s"), SpatialChId, *IChannelDataProvider::GetName(Provider));
				SetOwningChannelId(NetId, SpatialChId);
				AddProvider(SpatialChId, Provider);
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
	// Don't set NetId-ChannelId mapping or add the provider, as we don't have the spatial channelId of the object yet.
	// The spatial channelId will be queried in AddProviderToDefaultChannel()
	return true;
}

void USpatialChannelDataView::SendSpawnToConn(AActor* Actor, UChanneldNetConnection* NetConn, uint32 OwningConnId)
{
	// Location must be set for channeld to update the spatial channelId.
	FVector Location = Actor->GetActorLocation();
	// Set the PC's location to the start position, to make sure PC is in the same spatial channel as the pawn.
	if (Actor->IsA<APlayerController>())
	{
		AActor* StartSpot;
		Location = PlayerStartLocator->GetPlayerStartPosition(OwningConnId, StartSpot);
		Actor->SetActorLocation(Location);
	}
	NetConn->SendSpawnMessage(Actor, Actor->IsA<APlayerController>() ? ROLE_AutonomousProxy : Actor->GetRemoteRole(), GetChanneldSubsystem()->LowLevelSendToChannelId.Get(), OwningConnId, &Location);
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
	channeldpb::ServerForwardMessage ServerForwardMessage;
	ServerForwardMessage.set_payload(SpawnMsg.SerializeAsString());
	Connection->Send(SpatialChId, MessageType_SPAWN, ServerForwardMessage, static_cast<channeldpb::BroadcastType>(channeldpb::ADJACENT_CHANNELS | channeldpb::ALL_BUT_SENDER));
	UE_LOG(LogChanneld, Log, TEXT("[Server] Broadcasted Spawn message to spatial channels(%d), obj: %s, netId: %d"), SpatialChId, *Obj->GetName(), NetId.Value);

	NetDriver->SetAllSentSpawn(NetId);
}
