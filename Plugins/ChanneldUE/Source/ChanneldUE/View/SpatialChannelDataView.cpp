// Fill out your copyright notice in the Description page of Project Settings.


#include "View/SpatialChannelDataView.h"

#include "ChanneldGameInstanceSubsystem.h"
#include "ChanneldNetConnection.h"
#include "ChanneldNetDriver.h"
#include "ChanneldUtils.h"
#include "Net/DataChannel.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"

USpatialChannelDataView::USpatialChannelDataView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USpatialChannelDataView::InitServer()
{
	Connection->AddMessageHandler(channeldpb::SUB_TO_CHANNEL, [&](UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg)
	{
		auto SubResultMsg = static_cast<const channeldpb::SubscribedToChannelResultMessage*>(Msg);
        // A client is subscribed to a spatial channel the server owns (Sub message is sent by Master server)
		if (SubResultMsg->conntype() == channeldpb::CLIENT && SubResultMsg->channeltype() == channeldpb::SPATIAL && SubResultMsg->suboptions().dataaccess() == channeldpb::WRITE_ACCESS)
		{
			ensureMsgf(!ClientInChannels.Contains(SubResultMsg->connid()), TEXT("Client conn %d had already been added to this server"), SubResultMsg->connid());
			
			ClientInChannels.Add(SubResultMsg->connid(), ChId);

			if (auto NetDriver = GetChanneldSubsystem()->GetNetDriver())//NetDriver.IsValid())
			{
				UChanneldNetConnection* ClientConn = NetDriver->AddChanneldClientConnection(SubResultMsg->connid());
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

				GetWorld()->GetAuthGameMode()->RestartPlayer(ClientConn->PlayerController);
			}
			else
			{
				UE_LOG(LogChanneld, Error, TEXT("Spatial server failed to create client connection %d: NetDriver is not valid."), SubResultMsg->connid());
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
	
	Super::InitServer();
}

void USpatialChannelDataView::ClientHandleSubToChannel(UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto SubResultMsg = static_cast<const channeldpb::SubscribedToChannelResultMessage*>(Msg);
	if (SubResultMsg->channeltype() == channeldpb::SPATIAL && SubResultMsg->suboptions().dataaccess() == channeldpb::WRITE_ACCESS)
	{
		if (bClientInMasterServer)
		{
			bClientInMasterServer = false;
			UE_LOG(LogChanneld, Log, TEXT("==================== Client no longer in Master server ===================="));
			
			GetChanneldSubsystem()->SetLowLevelSendToChannelId(ChId);
			// // Join the spatial server which Master server chooses for the player.
			// GetChanneldSubsystem()->OpenLevel(FName("127.0.0.1"));

			// Clear the actors (PlayerController, Pawn, PlayerState, etc.) spawned in master server.
			
		}
	}
}

void USpatialChannelDataView::InitClient()
{
	Connection->AddMessageHandler(channeldpb::SUB_TO_CHANNEL, this, &USpatialChannelDataView::ClientHandleSubToChannel);

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
	
	Super::InitClient();
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
				UE_LOG(LogChanneld, Log, TEXT("Queried spatial channelId %d for object: %s"), SpatialChId, *IChannelDataProvider::GetName(Provider));
				AddProvider(SpatialChId, Provider);
			});
		}
		else
		{
			UE_LOG(LogChanneld, Warning, TEXT("Failed to add provider: can't retrieve location from object: %s"), *IChannelDataProvider::GetName(Provider));
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
