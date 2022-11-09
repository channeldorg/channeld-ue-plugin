// Fill out your copyright notice in the Description page of Project Settings.


#include "View/SpatialMasterServerView.h"

#include "ChanneldGameInstanceSubsystem.h"
#include "ChanneldNetConnection.h"
#include "ChanneldUtils.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/KismetSystemLibrary.h"

USpatialMasterServerView::USpatialMasterServerView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USpatialMasterServerView::InitServer()
{
	// The player joins Master server first, for mapping player start position -> spatial channel id.
	// In order to spawn the player pawn at the same location in the spatial server as in Master server, some information should be shared between them.
	// After Master server subscribes the player to corresponding spatial channels, the player leaves Master server and travels to the spatial server.
	FGameModeEvents::GameModePostLoginEvent.AddLambda([&](AGameModeBase* GameMode, APlayerController* NewPlayer)
	{
		//APawn* NewPawn = NewPlayer->GetPawn();
		//FVector StartPos = NewPawn->GetActorLocation();
		AActor* PlayerStart = GameMode->FindPlayerStart(NewPlayer);
		check(PlayerStart);
		FVector StartPos = PlayerStart->GetActorLocation();
		
	/*
	// The spatial server has player pawn created. Master server decides which spatial channels the player subscribes based on the pawn's position.
	Connection->RegisterMessageHandler(MessageType_SERVER_PLAYER_SPAWNED, new unrealpb::ServerSpawnedPlayerMessage,
		[&](UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg)
	{
		const auto PlayerSpawnedMsg = static_cast<const unrealpb::ServerSpawnedPlayerMessage*>(Msg);
		const FVector StartPos = ChanneldUtils::GetVector(PlayerSpawnedMsg->startpos());
	*/
		TArray<FVector> Positions;
		Positions.Add(StartPos);
		// Query the channels from channeld.
		Connection->QuerySpatialChannel(Positions, [&, NewPlayer](const channeldpb::QuerySpatialChannelResultMessage* QueryResultMsg)
		{
			ChannelId StartChannelId = QueryResultMsg->channelid(0);
			if (StartChannelId == 0)
			{
				UE_LOG(LogChanneld, Error, TEXT("Unable to map the player start position %s to a spatial channel id"), *StartPos.ToCompactString());
				return;
			}

			channeldpb::ChannelSubscriptionOptions AuthoritySubOptions;
			AuthoritySubOptions.set_dataaccess(channeldpb::WRITE_ACCESS);
			AuthoritySubOptions.set_fanoutintervalms(ClientFanOutIntervalMs);
			AuthoritySubOptions.set_fanoutdelayms(ClientFanOutDelayMs);

			channeldpb::ChannelSubscriptionOptions NonAuthoritySubOptions;
			NonAuthoritySubOptions.set_dataaccess(channeldpb::READ_ACCESS);
			NonAuthoritySubOptions.set_fanoutintervalms(ClientFanOutIntervalMs);
			NonAuthoritySubOptions.set_fanoutdelayms(ClientFanOutDelayMs);

			// uint32 ClientConnId = PlayerSpawnedMsg->clientconnid();
			uint32 ClientConnId = CastChecked<UChanneldNetConnection>(NewPlayer->GetNetConnection())->GetConnId();
			
            // FIXME: should only sub to adjacent spatial channels. QuerySpatialChannelResultMessage should contains that information.
			for (const auto SpatialChId : AllSpatialChannelIds)
			{
				Connection->SubConnectionToChannel(ClientConnId, SpatialChId,
					SpatialChId == StartChannelId ? &AuthoritySubOptions : &NonAuthoritySubOptions);
			}
		});

		// // No need to spawn or sync any states.
		// GameMode->Logout((NewPlayer));
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
}
