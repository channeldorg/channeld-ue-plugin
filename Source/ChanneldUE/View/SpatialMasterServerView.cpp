#include "View/SpatialMasterServerView.h"

#include "ChanneldGameInstanceSubsystem.h"
#include "ChanneldNetConnection.h"
#include "ChanneldUtils.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Replication/ChanneldReplicationComponent.h"

USpatialMasterServerView::USpatialMasterServerView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USpatialMasterServerView::InitServer()
{
	Super::InitServer();
	
	if (UClass* PlayerStartLocatorClass = GetMutableDefault<UChanneldSettings>()->PlayerStartLocatorClass)
	{
		PlayerStartLocator = NewObject<UPlayerStartLocatorBase>(this, PlayerStartLocatorClass);
	}
	ensureAlwaysMsgf(PlayerStartLocator, TEXT("SpatialMasterServerView has not PlayerStartLocatorClass set."));
	
	// Master server won't create pawn for the players.
	GetWorld()->GetAuthGameMode()->DefaultPawnClass = NULL;

	Connection->AddMessageHandler(channeldpb::SUB_TO_CHANNEL, [&](UChanneldConnection* _, Channeld::ChannelId ChId, const google::protobuf::Message* Msg)
	{
		auto SubResultMsg = static_cast<const channeldpb::SubscribedToChannelResultMessage*>(Msg);
		UE_LOG(LogChanneld, Log, TEXT("[Server] Sub %s conn %d to %s channel %d"),
			SubResultMsg->conntype() == channeldpb::CLIENT ? TEXT("client") : TEXT("server"),
			SubResultMsg->connid(),
			UTF8_TO_TCHAR(channeldpb::ChannelType_Name(SubResultMsg->channeltype()).c_str()),
			ChId);
		
		// A client subs to GLOBAL - choose the start position and sub the client to corresponding spatial channels.
		if (SubResultMsg->conntype() == channeldpb::CLIENT && SubResultMsg->channeltype() == channeldpb::GLOBAL)
		{
			Channeld::ConnectionId ClientConnId = SubResultMsg->connid();
			AActor* StartSpot;
			FVector StartPos = PlayerStartLocator->GetPlayerStartPosition(ClientConnId, StartSpot);
			UE_LOG(LogChanneld, Log, TEXT("%s selected %s for client %d"), *PlayerStartLocator->GetName(), *StartPos.ToCompactString(), ClientConnId);
			
			TArray<FVector> Positions;
			Positions.Add(StartPos);
			
			// Query the channels from channeld.
			Connection->QuerySpatialChannel(Positions, [&, ClientConnId](const channeldpb::QuerySpatialChannelResultMessage* QueryResultMsg)
			{
				Channeld::ChannelId StartChannelId = QueryResultMsg->channelid(0);
				if (StartChannelId == 0)
				{
					UE_LOG(LogChanneld, Error, TEXT("Unable to map the player start position %s to a spatial channel id"), *StartPos.ToCompactString());
					return;
				}

				/*
				channeldpb::ChannelSubscriptionOptions AuthoritySubOptions;
				AuthoritySubOptions.set_dataaccess(channeldpb::WRITE_ACCESS);
				AuthoritySubOptions.set_fanoutintervalms(ClientFanOutIntervalMs);
				AuthoritySubOptions.set_fanoutdelayms(ClientFanOutDelayMs);
				*/
				channeldpb::ChannelSubscriptionOptions NonAuthoritySubOptions;
				NonAuthoritySubOptions.set_dataaccess(channeldpb::READ_ACCESS);
				NonAuthoritySubOptions.set_fanoutintervalms(ClientFanOutIntervalMs);
				NonAuthoritySubOptions.set_fanoutdelayms(ClientFanOutDelayMs);

				// The start spatial channelId MUST be sent at first.
				Connection->SubConnectionToChannel(ClientConnId, StartChannelId, &NonAuthoritySubOptions);

				/* The spatial server will sub the client to the spatial channels according to the interest settings.
				// Delay the sub of other spatial channels, so the client can treat the first spatial channelId as the one to log in.
				FTimerHandle Handle;
				GetWorld()->GetTimerManager().SetTimer(Handle, [&, StartChannelId, ClientConnId, NonAuthoritySubOptions]()
				{
					for (const auto SpatialChId : AllSpatialChannelIds)
					{
						if (SpatialChId != StartChannelId)
						{
							Connection->SubConnectionToChannel(ClientConnId, SpatialChId, &NonAuthoritySubOptions);
						}
					}
				}, 1, false, 0.5f);
				*/
			});
		}
	});

	Connection->AddMessageHandler(channeldpb::UNSUB_FROM_CHANNEL, [&](UChanneldConnection* _, Channeld::ChannelId ChId, const google::protobuf::Message* Msg)
	{
		auto UnsubMsg = static_cast<const channeldpb::UnsubscribedFromChannelResultMessage*>(Msg);
		if (UnsubMsg->channeltype() == channeldpb::GLOBAL && UnsubMsg->conntype() == channeldpb::CLIENT)
		{
			// Broadcast the unsub message to all other servers so they can remove the client connection.
			Connection->Broadcast(Channeld::GlobalChannelId, unrealpb::SERVER_PLAYER_LEAVE, *UnsubMsg, channeldpb::ALL_BUT_CLIENT | channeldpb::ALL_BUT_SENDER);
			/* Should not only send to spatial servers. The sub-world servers may also need to know the player leave.
			TArray<Channeld::ConnectionId> SpatialServerConnIds;
			AllSpatialChannelIds.GenerateValueArray(SpatialServerConnIds);
			for (auto ConnId : SpatialServerConnIds)
			{
				Connection->Forward(*AllSpatialChannelIds.FindKey(ConnId), unrealpb::SERVER_PLAYER_LEAVE, *UnsubMsg);
			}
			*/
			UE_LOG(LogChanneld, Log, TEXT("Broadcasted SERVER_PLAYER_LEAVE to all spatial servers, client connId: %d"), UnsubMsg->connid());
		}
	});

	Connection->AddMessageHandler(channeldpb::CREATE_SPATIAL_CHANNEL, [&](UChanneldConnection* _, Channeld::ChannelId ChId, const google::protobuf::Message* Msg)
	{
		auto ResultMsg = static_cast<const channeldpb::CreateSpatialChannelsResultMessage*>(Msg);
		for (auto SpatialChId : ResultMsg->spatialchannelid())
		{
			AllSpatialChannelIds.Emplace(SpatialChId, ResultMsg->ownerconnid());
			UE_LOG(LogChanneld, Verbose, TEXT("Added spatial channel %d"), SpatialChId);
		}
	});

	Connection->AddMessageHandler(channeldpb::REMOVE_CHANNEL, [&](UChanneldConnection* _, Channeld::ChannelId ChId, const google::protobuf::Message* Msg)
	{
		auto RemoveMsg = static_cast<const channeldpb::RemoveChannelMessage*>(Msg);
		AllSpatialChannelIds.Remove(RemoveMsg->channelid());
	});

	channeldpb::ChannelSubscriptionOptions GlobalSubOptions;
	GlobalSubOptions.set_dataaccess(channeldpb::WRITE_ACCESS);
	GlobalSubOptions.set_fanoutintervalms(ClientFanOutIntervalMs);
	GlobalSubOptions.set_fanoutdelayms(ClientFanOutDelayMs);
	GlobalSubOptions.set_skipselfupdatefanout(true);
	Connection->CreateChannel(channeldpb::GLOBAL, UKismetSystemLibrary::GetGameName(), &GlobalSubOptions, nullptr, nullptr,
		[&](const channeldpb::CreateChannelResultMessage* Msg)
		{
			GetChanneldSubsystem()->SetLowLevelSendToChannelId(Channeld::GlobalChannelId);
		});
}

void USpatialMasterServerView::AddProvider(Channeld::ChannelId ChId, IChannelDataProvider* Provider)
{
	// Should only replicates the GameStateBase
	if (!Provider->GetTargetObject()->IsA(AGameStateBase::StaticClass()))
	{
		return;
	}

	Super::AddProvider(ChId, Provider);
}

Channeld::ChannelId USpatialMasterServerView::GetOwningChannelId(const FNetworkGUID NetId) const
{
	return Channeld::GlobalChannelId;
}

void USpatialMasterServerView::OnClientPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer, UChanneldNetConnection* NewPlayerConn)
{
	// Send the GameStateBase to the new player
	auto Comp = Cast<UChanneldReplicationComponent>(NewPlayer->GetComponentByClass(UChanneldReplicationComponent::StaticClass()));
	if (Comp)
	{
		NewPlayerConn->SendSpawnMessage(GameMode->GameState, ENetRole::ROLE_SimulatedProxy);
	}
	else
	{
		UE_LOG(LogChanneld, Warning, TEXT("PlayerController is missing UChanneldReplicationComponent. Failed to spawn the GameStateBase in the client."));
	}
}
