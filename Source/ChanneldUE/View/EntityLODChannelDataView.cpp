#include "EntityLODChannelDataView.h"

#include "ChanneldGameInstanceSubsystem.h"
#include "ChanneldNetDriver.h"
#include "ChanneldTypes.h"
#include "ChanneldUtils.h"
#include "Engine/PackageMapClient.h"
#include "Replication/ChanneldActorReplicator.h"

UEntityLODChannelDataView::UEntityLODChannelDataView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

Channeld::ChannelId UEntityLODChannelDataView::GetOwningChannelId(UObject* Obj) const
{
	// GameState is owned by the Master server.
	if (Obj->IsA<AGameStateBase>())
	{
		return Channeld::GlobalChannelId;
	}
	return Super::GetOwningChannelId(Obj);
}

bool UEntityLODChannelDataView::OnServerSpawnedObject(UObject* Obj, const FNetworkGUID NetId)
{
	if (!NetId.IsValid())
		return false;

	SetOwningChannelId(NetId, NetId.Value);

	AddObjectProvider(NetId.Value, Obj);

	return true;
}

void UEntityLODChannelDataView::InitServer()
{
	// auto GameState = GetWorld()->GetAuthGameMode()->GameState;
	// SetOwningChannelId(GetNetId(CastChecked<UObject>(GameState)), Channeld::GameStateNetId);

	Super::InitServer();

	Connection->CreateChannel(channeldpb::GLOBAL, Metadata, nullptr, nullptr, nullptr,
		[&](const channeldpb::CreateChannelResultMessage* ResultMsg)
		{
			GetChanneldSubsystem()->SetLowLevelSendToChannelId(ResultMsg->channelid());
		});
}

void UEntityLODChannelDataView::InitClient()
{
	Super::InitClient();
	
	channeldpb::ChannelSubscriptionOptions GlobalSubOptions;
	GlobalSubOptions.set_dataaccess(channeldpb::READ_ACCESS);
	GlobalSubOptions.set_fanoutintervalms(GlobalChannelFanOutIntervalMs);
	GlobalSubOptions.set_fanoutdelayms(GlobalChannelFanOutDelayMs);
	Connection->SubToChannel(Channeld::GlobalChannelId, &GlobalSubOptions, [&](const channeldpb::SubscribedToChannelResultMessage* ResultMsg)
	{
		GetChanneldSubsystem()->SetLowLevelSendToChannelId(Channeld::GlobalChannelId);
	});

	FChanneldActorReplicator::OnReplicatedLocation.AddUObject(this, &UEntityLODChannelDataView::OnReplicatedActorLocation);
}

void UEntityLODChannelDataView::ServerHandleClientUnsub(Channeld::ConnectionId ClientConnId, channeldpb::ChannelType ChannelType, Channeld::ChannelId ChId)
{
	// A client leaves the game - close and remove the client connection.
	if (ChannelType == channeldpb::GLOBAL)
	{
		Super::ServerHandleClientUnsub(ClientConnId, ChannelType, ChId);
	}
}

void UEntityLODChannelDataView::SendSpawnToConn(UObject* Obj, UChanneldNetConnection* NetConn, uint32 OwningConnId)
{	
	// GameState is spawned via GLOBAL channel
	if (Obj->IsA<AGameStateBase>())
	{
		NetConn->SendSpawnMessage(Obj, ROLE_SimulatedProxy, Channeld::GlobalChannelId);
		return;
	}

	const uint32 ChId = GetOwningChannelId(Obj);
	// The entity channel already exists, send directly
	if (Connection->SubscribedChannels.Contains(ChId))
	{
		UChannelDataView::SendSpawnToConn(Obj, NetConn, OwningConnId);
		return;
	}
	
	const FNetworkGUID NetId = GetNetId(Obj, true);

	// Create the entity channel before sending spawn message
	channeldpb::ChannelSubscriptionOptions SubOptions;
	SubOptions.set_dataaccess(channeldpb::WRITE_ACCESS);
	Connection->CreateEntityChannel(Channeld::GlobalChannelId, Obj, NetId.Value, TEXT(""), &SubOptions, GetEntityData(Obj)/*nullptr*/, nullptr,
		[this, Obj, NetId, NetConn, OwningConnId](const channeldpb::CreateChannelResultMessage* ResultMsg)
		{
			SetOwningChannelId(NetId, NetId.Value);
			AddObjectProvider(ResultMsg->channelid(), Obj);

			UChannelDataView::SendSpawnToConn(Obj, NetConn, OwningConnId);
		});

	UE_LOG(LogChanneld, Verbose, TEXT("Creating entity channel for obj: %s, netId: %d, owningConnId: %d"), *Obj->GetName(), NetId.Value, OwningConnId);
}

void UEntityLODChannelDataView::SendSpawnToClients_EntityChannelReady(const FNetworkGUID NetId, UObject* Obj, uint32 OwningConnId)
{
	unrealpb::SpawnObjectMessage SpawnMsg;
	SpawnMsg.set_channelid(NetId.Value);
	// As we don't have any specific NetConnection to export the NetId, use a virtual one.
	SpawnMsg.mutable_obj()->CopyFrom(*ChanneldUtils::GetRefOfObject(Obj, NetConnForSpawn, true));
	// ensureAlwaysMsgf(SpawnMsg.mutable_obj()->context_size() > 0, TEXT("Spawn message has no context! NetId: %d"), NetId.Value);

	if (const AActor* Actor = Cast<AActor>(Obj))
	{
		// Broadcast the EntityChannelCreated event to the actor so it can do some initialization.
		if (auto RepComp = Cast<UChanneldReplicationComponent>(Actor->GetComponentByClass(UChanneldReplicationComponent::StaticClass())))
		{
			RepComp->OnEntityChannelCreated.Broadcast(NetId.Value);
		}
				
		SpawnMsg.set_localrole(Actor->GetRemoteRole());
		if (OwningConnId > 0)
		{
			SpawnMsg.mutable_obj()->set_owningconnid(OwningConnId);
		}
		
		/* The client needs to know the location for the LOD-based sub,
		   but channeld will use the location to calculate the spatial channel id, which is not what we want.
		   So we skip this field and do the LOD-based sub on the first fan-out.
		SpawnMsg.mutable_location()->MergeFrom(ChanneldUtils::GetVectorPB(Actor->GetActorLocation()));
		*/
	}
	
	// Broadcast the spawn message to all client connections as all entities are Well-Known Object.
	Connection->Broadcast(Channeld::GlobalChannelId, unrealpb::SPAWN, SpawnMsg, channeldpb::ALL_BUT_SERVER);
	UE_LOG(LogChanneld, Log, TEXT("[Server] Broadcasted Spawn message of well-known obj: %s, netId: %d"), *Obj->GetName(), NetId.Value);
			
	GetChanneldSubsystem()->GetNetDriver()->SetAllSentSpawn(NetId);
}

// Broadcast the spawning to all clients
void UEntityLODChannelDataView::SendSpawnToClients(UObject* Obj, uint32 OwningConnId)
{
	const auto NetDriver = GetChanneldSubsystem()->GetNetDriver();
	if (!NetDriver)
	{
		UE_LOG(LogChanneld, Error, TEXT("UEntityLODChannelDataView::SendSpawnToClients: Unable to get ChanneldNetDriver"));
		return;
	}

	const Channeld::ChannelId ChId = GetOwningChannelId(Obj);
	const FNetworkGUID NetId = NetDriver->GuidCache->GetOrAssignNetGUID(Obj);
	
	// The entity channel already exists, send directly
	if (Connection->SubscribedChannels.Contains(ChId))
	{
		SendSpawnToClients_EntityChannelReady(NetId, Obj, OwningConnId);
		return;
	}

	channeldpb::ChannelSubscriptionOptions SubOptions;
	SubOptions.set_dataaccess(channeldpb::WRITE_ACCESS);
	Connection->CreateEntityChannel(Channeld::GlobalChannelId, Obj, NetId.Value, TEXT(""), &SubOptions, GetEntityData(Obj)/*nullptr*/, nullptr,
		[this, NetId, Obj, OwningConnId](const channeldpb::CreateChannelResultMessage* _)
		{
			SetOwningChannelId(NetId, NetId.Value);
			AddObjectProvider(NetId.Value, Obj);
			
			SendSpawnToClients_EntityChannelReady(NetId, Obj, OwningConnId);
		});

	UE_LOG(LogChanneld, Verbose, TEXT("Creating entity channel for obj: %s, netId: %d, owningConnId: %d"), *Obj->GetName(), NetId.Value, OwningConnId);
}

// Client subscribes to the spawned entity channel
void UEntityLODChannelDataView::OnNetSpawnedObject(UObject* Obj, const Channeld::ChannelId ChId, const unrealpb::SpawnObjectMessage* SpawnMsg)
{
	if (!SpawnMsg)
	{
		UE_LOG(LogChanneld, Warning, TEXT("Entity should always be spawned from SpawnMsg. NetId: %d"), ChId);
		return;
	}

	uint32 FanOutInterval = GlobalChannelFanOutIntervalMs;
	channeldpb::ChannelSubscriptionOptions SubOptions;
	SubOptions.set_fanoutintervalms(FanOutInterval);
	SubOptions.set_dataaccess(channeldpb::READ_ACCESS);
	Connection->SubToChannel(ChId, &SubOptions);
}

void UEntityLODChannelDataView::OnDestroyedActor(AActor* Actor, const FNetworkGUID NetId)
{
	// Unsub from the entity channel which channelId equals to the NetId
	if (Connection->SubscribedChannels.Contains(NetId.Value))
	{
		Connection->UnsubFromChannel(NetId.Value);
	}
	
	Super::OnDestroyedActor(Actor, NetId);
}

void UEntityLODChannelDataView::SendDestroyToClients(UObject* Obj, const FNetworkGUID NetId)
{
	const auto NetDriver = GetChanneldSubsystem()->GetNetDriver();
	if (!NetDriver)
	{
		UE_LOG(LogChanneld, Error, TEXT("UEntityLODChannelDataView::SendDestroyToClients: Unable to get ChanneldNetDriver"));
		return;
	}

	int BroadcastType = channeldpb::ALL_BUT_SENDER;
	// Don't broadcast the destroy of objects that are only spawned in the owning client.
	if (/*Obj->IsA<APlayerState>() ||*/ Obj->IsA<APlayerController>())
	{
		BroadcastType |= channeldpb::ALL_BUT_CLIENT;
	}
	
	unrealpb::DestroyObjectMessage DestroyMsg;
	DestroyMsg.set_netid(NetId.Value);
	DestroyMsg.set_reason(static_cast<uint8>(EChannelCloseReason::Destroyed));
	Connection->Broadcast(Channeld::GlobalChannelId, unrealpb::DESTROY, DestroyMsg, BroadcastType);
	UE_LOG(LogChanneld, Log, TEXT("[Server] Broadcasted Destroy message to GLOBAL channel, obj: %s, netId: %d"), *GetNameSafe(Obj), NetId.Value);
}

// Update sub options to the entity channel based on the distance to the player (LOD)
void UEntityLODChannelDataView::OnReplicatedActorLocation(AActor* Actor, FVector& NewLocation)
{
	auto NetDriver = GetChanneldSubsystem()->GetNetDriver();
	if (!NetDriver)
	{
		return;
	}
	
	auto ServerConn = NetDriver->GetServerConnection();
	if (!ServerConn)
	{
		return;
	}
	
	auto PC = ServerConn->PlayerController;
	if (!PC)
	{
		UE_LOG(LogChanneld, Warning, TEXT("Unable to get PlayerController from ServerConnection %d"), ServerConn->GetConnId());
		return;
	}
	
	auto Pawn = PC->GetPawn();
	if (!Pawn)
	{
		return;
	}

	const Channeld::ChannelId ChId = GetOwningChannelId(Actor);
	if (ChId == Channeld::InvalidChannelId)
	{
		UE_LOG(LogChanneld, Warning, TEXT("Failed to update LOD of '%s' as it doesn't have owning channel"), *GetNameSafe(Actor));
		return;
	}
	auto SubInfo = Connection->SubscribedChannels.Find(ChId);
	if (!SubInfo)
	{
		UE_LOG(LogChanneld, Warning, TEXT("Failed to update LOD of '%s' as it's not subscribed to channel %d"), *GetNameSafe(Actor), ChId);
		return;
	}
		
	uint32 FanOutInterval = GlobalChannelFanOutIntervalMs;
	float Dist = FVector::Distance(Pawn->GetActorLocation(), NewLocation);
	bool bFound = false;
	for (auto& Def : LOD_Definitions)
	{
		if (Dist < Def.Distance)
		{
			FanOutInterval = Def.FanOutInterval;
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		UE_LOG(LogChanneld, Warning, TEXT("Failed to update LOD as no definition found for distance %2f"), Dist);
		return;
	}
	// No change
	if (SubInfo->SubOptions.FanOutIntervalMs == FanOutInterval)
	{
		return;
	}
	SubInfo->SubOptions.FanOutIntervalMs = FanOutInterval;

	UE_LOG(LogChanneld, Verbose, TEXT("Updating FanOutInterval of %s to: %d"), *GetNameSafe(Actor), FanOutInterval);
	channeldpb::ChannelSubscriptionOptions SubOptions;
	SubOptions.set_fanoutintervalms(FanOutInterval);
	Connection->SubToChannel(ChId, &SubOptions);
}