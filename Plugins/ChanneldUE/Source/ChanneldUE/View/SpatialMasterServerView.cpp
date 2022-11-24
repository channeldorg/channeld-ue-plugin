// Fill out your copyright notice in the Description page of Project Settings.


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
	// Master server won't create pawn for the players.
	GetWorld()->GetAuthGameMode()->DefaultPawnClass = NULL;

	Connection->AddMessageHandler(channeldpb::SUB_TO_CHANNEL, [&](UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg)
	{
		auto SubResultMsg = static_cast<const channeldpb::SubscribedToChannelResultMessage*>(Msg);
		// A client subs to GLOBAL - choose the start position and sub the client to corresponding spatial channels.
		if (SubResultMsg->conntype() == channeldpb::CLIENT && SubResultMsg->channeltype() == channeldpb::GLOBAL)
		{
			FVector StartPos = FVector::ZeroVector;
			if (auto Itr = TActorIterator<APlayerStart>(GetWorld()))
			{
				StartPos = Itr->GetActorLocation();
			}

			ConnectionId ClientConnId = SubResultMsg->connid();
			
			/*
		}
	});
	
	// The player joins Master server first, for mapping player start position -> spatial channel id.
	// In order to spawn the player pawn at the same location in the spatial server as in Master server, some information should be shared between them.
	// After Master server subscribes the player to corresponding spatial channels, the player leaves Master server and travels to the spatial server.
	FGameModeEvents::GameModePostLoginEvent.AddLambda([&](AGameModeBase* GameMode, APlayerController* NewPlayer)
	{
		ConnectionId ClientConnId = CastChecked<UChanneldNetConnection>(NewPlayer->GetNetConnection())->GetConnId();
		
		AActor* PlayerStart = GameMode->FindPlayerStart(NewPlayer);
		check(PlayerStart);
		FVector StartPos = PlayerStart->GetActorLocation();
		*/
		TArray<FVector> Positions;
		Positions.Add(StartPos);

			
		// Query the channels from channeld.
		Connection->QuerySpatialChannel(Positions, [&, ClientConnId](const channeldpb::QuerySpatialChannelResultMessage* QueryResultMsg)
		{
			ChannelId StartChannelId = QueryResultMsg->channelid(0);
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

			// FIXME: should only sub to adjacent spatial channels. QuerySpatialChannelResultMessage should contains that information.
			for (const auto SpatialChId : AllSpatialChannelIds)
			{
				Connection->SubConnectionToChannel(ClientConnId, SpatialChId,
					/*SpatialChId == StartChannelId ? &AuthoritySubOptions :*/ &NonAuthoritySubOptions);
			}

			/*
			// At this moment, the player Pawn and PlayerState should be removed on both sever and client sides as it will be re-created on the spatial server.
			// The PlayerController should stay in the Master server for further RPC call.
			if (APawn* Pawn = NewPlayer->GetPawn())
			{
				NewPlayer->UnPossess();
				GetWorld()->DestroyActor(Pawn);
			}
			*/
		});
		}
	});

	Connection->AddMessageHandler(channeldpb::CREATE_SPATIAL_CHANNEL, [&](UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg)
	{
		auto ResultMsg = static_cast<const channeldpb::CreateSpatialChannelsResultMessage*>(Msg);
		for (auto SpatialChId : ResultMsg->spatialchannelid())
		{
			AllSpatialChannelIds.Add(SpatialChId);
		}
	});

	Connection->AddMessageHandler(channeldpb::REMOVE_CHANNEL, [&](UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg)
	{
		auto RemoveMsg = static_cast<const channeldpb::RemoveChannelMessage*>(Msg);
		AllSpatialChannelIds.Remove(RemoveMsg->channelid());
	});

	channeldpb::ChannelSubscriptionOptions GlobalSubOptions;
	GlobalSubOptions.set_dataaccess(channeldpb::WRITE_ACCESS);
	GlobalSubOptions.set_fanoutintervalms(ClientFanOutIntervalMs);
	GlobalSubOptions.set_fanoutdelayms(ClientFanOutDelayMs);
	Connection->CreateChannel(channeldpb::GLOBAL, UKismetSystemLibrary::GetGameName(), &GlobalSubOptions, nullptr, nullptr,
		[&](const channeldpb::CreateChannelResultMessage* Msg)
		{
			GetChanneldSubsystem()->SetLowLevelSendToChannelId(GlobalChannelId);
		});

	Super::InitServer();
}

void USpatialMasterServerView::AddProvider(ChannelId ChId, IChannelDataProvider* Provider)
{
	/*
	// Should only replicates the GameStateBase
	if (!Provider->GetTargetObject()->IsA(AGameStateBase::StaticClass()))
	{
		return;
	}

	Super::AddProvider(ChId, Provider);
	*/
}

ChannelId USpatialMasterServerView::GetOwningChannelId(const FNetworkGUID NetId) const
{
	return GlobalChannelId;
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
