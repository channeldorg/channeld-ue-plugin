#include "ChannelDataView.h"
#include "ChanneldGameInstanceSubsystem.h"
#include "ChanneldNetDriver.h"
#include "ChanneldUtils.h"
#include "Metrics.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Replication/ChanneldReplicationComponent.h"

UChannelDataView::UChannelDataView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UChannelDataView::RegisterChannelDataType(EChanneldChannelType ChannelType, const FString& MessageFullName)
{
	auto Msg = ChanneldUtils::CreateProtobufMessage(TCHAR_TO_UTF8(*MessageFullName));
	if (Msg)
	{
		RegisterChannelDataTemplate(static_cast<channeldpb::ChannelType>(ChannelType), Msg);
	}
	else
	{
		UE_LOG(LogChanneld, Error, TEXT("Failed to register channel data type by name: %s"), *MessageFullName);
	}
}

void UChannelDataView::Initialize(UChanneldConnection* InConn)
{
	if (Connection == nullptr)
	{
		Connection = InConn;

		LoadCmdLineArgs();

		Connection->AddMessageHandler(channeldpb::CHANNEL_DATA_UPDATE, this, &UChannelDataView::HandleChannelDataUpdate);
		Connection->AddMessageHandler(channeldpb::UNSUB_FROM_CHANNEL, this, &UChannelDataView::HandleUnsub);
	}

	if (Connection->IsServer())
	{
		const float InitDelay = GetMutableDefault<UChanneldSettings>()->DelayViewInitInSeconds;
		if (InitDelay > 0)
		{
			FTimerHandle Handle;
			GetWorld()->GetTimerManager().SetTimer(Handle, [&](){InitServer();}, 1, false, InitDelay);
		}
		else
		{
			InitServer();
		}
	}
	else if (Connection->IsClient())
	{
		InitClient();
	}
	else
	{
		UE_LOG(LogChanneld, Warning, TEXT("Invalid connection type: %s"), 
			UTF8_TO_TCHAR(channeldpb::ConnectionType_Name(Connection->GetConnectionType()).c_str()));
		return;
	}
	
	UE_LOG(LogChanneld, Log, TEXT("%s initialized channels."), *this->GetClass()->GetName());
}

void UChannelDataView::InitServer()
{
	// Add the GameStateBase (if it's an IChannelDataProvider).
	// Missing this step will cause client failing to begin play.
	AddActorProvider(GlobalChannelId, GetWorld()->GetAuthGameMode()->GameState);
	
	ReceiveInitServer();
}

void UChannelDataView::InitClient()
{
	ReceiveInitClient();
}

void UChannelDataView::UninitServer()
{
	ReceiveUninitServer();
}

void UChannelDataView::UninitClient()
{
	ReceiveUninitClient();
}

void UChannelDataView::Unintialize()
{
	// NetDriver = nullptr;
	
	if (Connection != nullptr)
	{
		Connection->RemoveMessageHandler(channeldpb::CHANNEL_DATA_UPDATE, this);
	}
	else
	{
		return;
	}

	if (Connection->IsServer())
	{
		UninitServer();
	}
	else if (Connection->IsClient())
	{
		UninitClient();
	}
	else
	{
		UE_LOG(LogChanneld, Warning, TEXT("Invalid connection type: %s"),
			UTF8_TO_TCHAR(channeldpb::ConnectionType_Name(Connection->GetConnectionType()).c_str()));
		return;
	}

	UE_LOG(LogChanneld, Log, TEXT("%s uninitialized channels."), *this->GetClass()->GetName());
}

void UChannelDataView::BeginDestroy()
{
	/*
	*/
	delete AnyForTypeUrl;

	for (auto Itr = ChannelDataTemplatesByTypeUrl.CreateIterator(); Itr; ++Itr)
	{
		delete Itr.Value();
	}
	// for (auto& Pair : ChannelDataTemplatesByTypeUrl)
	// {
	// 	delete Pair.Value;
	// }
	ChannelDataTemplatesByTypeUrl.Empty();

	for (auto Itr = ReceivedUpdateDataInChannels.CreateIterator(); Itr; ++Itr)
	{
		delete Itr.Value();
	}
	// for (auto& Pair : ReceivedUpdateDataInChannels)
	// {
	// 	delete Pair.Value;
	// }
	ReceivedUpdateDataInChannels.Empty();

	for (auto Itr = RemovedProvidersData.CreateIterator(); Itr; ++Itr)
	{
		delete Itr.Value();
	}
	// for (auto& Pair : RemovedProvidersData)
	// {
	// 	delete Pair.Value;
	// }
	RemovedProvidersData.Empty();
	
	ChannelDataProviders.Empty();

	Super::BeginDestroy();
}

void UChannelDataView::AddProvider(ChannelId ChId, IChannelDataProvider* Provider)
{
	/*
	ensureMsgf(Provider->GetChannelType() != channeldpb::UNKNOWN, TEXT("Invalid channel type of data provider: %s"), *IChannelDataProvider::GetName(Provider));
	if (!ChannelDataTemplates.Contains(Provider->GetChannelType()))
	{
		RegisterChannelDataTemplate(Provider->GetChannelType(), Provider->GetChannelDataTemplate());
	}
	*/

	TSet<FProviderInternal>& Providers = ChannelDataProviders.FindOrAdd(ChId);
	if (!Providers.Contains(Provider))
	{
		Providers.Add(Provider);
		UE_LOG(LogChanneld, Log, TEXT("Added channel data provider %s to channel %d"), *IChannelDataProvider::GetName(Provider), ChId);
	}
	ChannelDataProviders[ChId] = Providers;
}

void UChannelDataView::AddProviderToDefaultChannel(IChannelDataProvider* Provider)
{
	FNetworkGUID NetId = GetNetId(Provider);
	if (NetId.IsValid())
	{
		ChannelId ChId = GetOwningChannelId(NetId);
		if (ChId != InvalidChannelId)
		{
			AddProvider(ChId, Provider);
		}
		else
		{
			UE_LOG(LogChanneld, Warning, TEXT("Failed to add provider: no channelId mapping found for object: %s"), *IChannelDataProvider::GetName(Provider));
		}			
	}
	else
	{
		UE_LOG(LogChanneld, Warning, TEXT("Failed to add provider: can't find NetGUID for object: %s"), *IChannelDataProvider::GetName(Provider));
	}
}

void UChannelDataView::AddActorProvider(ChannelId ChId, AActor* Actor)
{
	if (Actor == nullptr)
		return;
	
	if (Actor->Implements<UChannelDataProvider>())
	{
		AddProvider(ChId, Cast<IChannelDataProvider>(Actor));
	}
	for (auto Comp : Actor->GetComponentsByInterface(UChannelDataProvider::StaticClass()))
	{
		AddProvider(ChId, Cast<IChannelDataProvider>(Comp));
	}
}

void UChannelDataView::AddActorProvider(AActor* Actor)
{
	if (Actor->Implements<UChannelDataProvider>())
	{
		AddProviderToDefaultChannel(Cast<IChannelDataProvider>(Actor));
	}
	for (const auto Comp : Actor->GetComponentsByInterface(UChannelDataProvider::StaticClass()))
	{
		AddProviderToDefaultChannel(Cast<IChannelDataProvider>(Comp));
	}
}

void UChannelDataView::RemoveProvider(ChannelId ChId, IChannelDataProvider* Provider, bool bSendRemoved)
{
	// ensureMsgf(Provider->GetChannelType() != channeldpb::UNKNOWN, TEXT("Invalid channel type of data provider: %s"), *IChannelDataProvider::GetName(Provider));
	TSet<FProviderInternal>* Providers = ChannelDataProviders.Find(ChId);
	if (Providers != nullptr)
	{
		UE_LOG(LogChanneld, Log, TEXT("Removing channel data provider %s from channel %d"), *IChannelDataProvider::GetName(Provider), ChId);
		
		if (bSendRemoved)
		{
			Provider->SetRemoved();

			// Collect the removed states immediately (before the provider gets destroyed completely)
			google::protobuf::Message* RemovedData = RemovedProvidersData.FindRef(ChId);
			if (!RemovedData)
			{
				EChanneldChannelType ChannelType = GetChanneldSubsystem()->GetChannelTypeByChId(ChId);
				if (ChannelType == EChanneldChannelType::ECT_Unknown)
				{
					UE_LOG(LogChanneld, Error, TEXT("Can't map channel type from channel id: %d. Removed states won't be created for provider: %s"), ChId, *IChannelDataProvider::GetName(Provider));
					Providers->Remove(Provider);
					return;
				}
				else
				{
					const auto MsgTemplate = ChannelDataTemplates.FindRef(static_cast<channeldpb::ChannelType>(ChannelType));
					if (!ensureMsgf(MsgTemplate, TEXT("Can't find channel data message template of channel type: %s"), *UEnum::GetValueAsString(ChannelType)))
					{
						Providers->Remove(Provider);
						return;
					}
					RemovedData = MsgTemplate->New();
					RemovedProvidersData.Add(ChId, RemovedData);
				}
			}
			Provider->UpdateChannelData(RemovedData);
		}
		else
		{
			Providers->Remove(Provider);
		}
	}
}

void UChannelDataView::RemoveProviderFromAllChannels(IChannelDataProvider* Provider, bool bSendRemoved)
{
	if (Connection == nullptr)
	{
		UE_LOG(LogChanneld, Error, TEXT("Unable to call UChannelDataView::RemoveProviderFromAllChannels. The connection to channeld hasn't been set up yet and there's no subscription to any channel."));
		return;
	}

	/*
	ensureMsgf(Provider->GetChannelType() != channeldpb::UNKNOWN, TEXT("Invalid channel type of data provider: %s"), *IChannelDataProvider::GetName(Provider));
	for (auto& Pair : Connection->SubscribedChannels)
	{
		if (static_cast<channeldpb::ChannelType>(Pair.Value.ChannelType) == Provider->GetChannelType())
		{
			RemoveProvider(Pair.Key, Provider, bSendRemoved);
			return;
		}
	}
	*/
	for (auto& Pair : ChannelDataProviders)
	{
		if (Pair.Value.Contains(Provider))
		{
			RemoveProvider(Pair.Key, Provider, bSendRemoved);
		}
	}
}

void UChannelDataView::MoveProvider(ChannelId OldChId, ChannelId NewChId, IChannelDataProvider* Provider)
{
	RemoveProvider(OldChId, Provider, false);
	if (Connection->SubscribedChannels.Contains(NewChId))
	{
		AddProvider(NewChId, Provider);
	}
}

void UChannelDataView::OnClientPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer, UChanneldNetConnection* NewPlayerConn)
{
	ensureMsgf(NewPlayer->GetComponentByClass(UChanneldReplicationComponent::StaticClass()), TEXT("PlayerController is missing UChanneldReplicationComponent. Failed to spawn PC, GameStateBase, and other pawns in the client."));
	/*
	// Actors may don't have UChanneldReplicationComponent created when OnServerSpawnedObject is called, so we should try to add the providers again.
	AddActorProvider(GameMode->GameState);
	AddActorProvider(NewPlayer);
	AddActorProvider(NewPlayer->PlayerState);
	*/
	
	// Send the GameStateBase to the new player. This is very important as later the GameStateBase will be fanned out to the client with bReplicatedHasBegunPlay=true, to trigger the client's BeginPlay().
	SendSpawnToConn(GameMode->GameState, NewPlayerConn, 0);
	
	/* Unfortunately, a couple of RPC on the PC is called in GameMode::PostLogin BEFORE invoking this event. So we need to handle the RPC properly.
	 * UPDATE: no need to worry - the client can queue unmapped RPC now!
	 */
	// Send the PC to the owning client after the PC and NetConn are mutually referenced.
	SendSpawnToConn(NewPlayer, NewPlayerConn, NewPlayerConn->GetConnId());
	SendSpawnToConn(NewPlayer->PlayerState, NewPlayerConn, NewPlayerConn->GetConnId());
	
	/* OnServerSpawnedActor() sends the spawning of the new player's pawn to other clients */

	// Send the existing player pawns to the new player
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PC = Iterator->Get();
		if (PC && PC != NewPlayer)
		{
			/* PC only exists in owning client as an AutonomousProxy!!!
			NewPlayerConn->SendSpawnMessage(PC, ROLE_SimulatedProxy);
			*/
			if (PC->GetPawn())
			{
				NewPlayerConn->SendSpawnMessage(PC->GetPawn(), ENetRole::ROLE_SimulatedProxy);
			}
		}
	}
}

FNetworkGUID UChannelDataView::GetNetId(UObject* Obj) const
{
	if (const auto NetDriver = GetChanneldSubsystem()->GetNetDriver())
	{
		// FIXME: should check the provider's authority instead.
		if (NetDriver->IsServer())
		{
			return NetDriver->GuidCache->GetOrAssignNetGUID(Obj);
		}
		else
		{
			return NetDriver->GuidCache->GetNetGUID(Obj);
		}
	}
	return FNetworkGUID();
}

FNetworkGUID UChannelDataView::GetNetId(IChannelDataProvider* Provider) const
{
	return GetNetId(Provider->GetTargetObject());
}

bool UChannelDataView::OnServerSpawnedObject(UObject* Obj, const FNetworkGUID NetId)
{
	if (!NetId.IsValid())
		return false;

	ChannelId ChId = GetChanneldSubsystem()->LowLevelSendToChannelId.Get();
	SetOwningChannelId(NetId, ChId);
	// NetIdOwningChannels.Add(NetId, ChId);
	// UE_LOG(LogChanneld, Log, TEXT("Set up mapping of netId: %d -> channelId: %d, spawned: %s"), NetId.Value, ChId, *GetNameSafe(Obj));

	if (Obj->IsA<AActor>())
	{
		AddActorProvider(ChId, Cast<AActor>(Obj));
	}

	return true;
}

void UChannelDataView::SendSpawnToConn(UObject* Obj, UChanneldNetConnection* NetConn, uint32 OwningConnId)
{
	ENetRole Role = ROLE_None;
	if (const AActor* Actor = Cast<AActor>(Obj))
	{
		Role = Actor->GetRemoteRole();
	}
	NetConn->SendSpawnMessage(Obj, Role, OwningConnId);
}

void UChannelDataView::OnDestroyedObject(UObject* Obj, const FNetworkGUID NetId)
{
	if (!NetId.IsValid())
		return;

	NetIdOwningChannels.Remove(NetId);

	if (Obj->IsA<AActor>())
	{
		if (Obj->Implements<UChannelDataProvider>())
		{
			RemoveProviderFromAllChannels(Cast<IChannelDataProvider>(Obj), false);
		}
		auto Providers = Cast<AActor>(Obj)->GetComponentsByInterface(UChannelDataProvider::StaticClass());
		for (const auto Provider : Providers)
		{
			RemoveProviderFromAllChannels(Cast<IChannelDataProvider>(Provider), false);
		}
	}
}

void UChannelDataView::SetOwningChannelId(const FNetworkGUID NetId, ChannelId ChId)
{
	if (!NetId.IsValid())
		return;
	
	NetIdOwningChannels.Add(NetId, ChId);
	UE_LOG(LogChanneld, Log, TEXT("Set up mapping of netId: %d (%d) -> channelId: %d"), NetId.Value, ChanneldUtils::GetNativeNetId(NetId.Value), ChId);
}

ChannelId UChannelDataView::GetOwningChannelId(const FNetworkGUID NetId) const
{
	const ChannelId* ChId = NetIdOwningChannels.Find(NetId);
	if (ChId)
	{
		return *ChId;
	}
	return InvalidChannelId;
}

ChannelId UChannelDataView::GetOwningChannelId(const AActor* Actor) const
{
	if (const auto NetConn = Actor->GetNetConnection())
	{
		if (const auto NetDriver = Cast<UChanneldNetDriver>(NetConn->Driver))
		{
			const FNetworkGUID NetId = NetDriver->GuidCache->GetNetGUID(Actor);
			if (NetId.IsValid())
			{
				return GetOwningChannelId(NetId);
			}
			else
			{
				UE_LOG(LogChanneld, Log, TEXT("No NetGUID has been assigned to Actor %s"), *GetNameSafe(Actor));
			}
		}
	}

	return InvalidChannelId;
}

void UChannelDataView::OnDisconnect()
{
	for (auto& Pair : ChannelDataProviders)
	{
		for (FProviderInternal& Provider : Pair.Value)
		{
			if (Provider.IsValid())
			{
				Provider->SetRemoved();
			}
		}
	}

	// Force to send the channel update data with the removed states to channeld
	SendAllChannelUpdates();
}

int32 UChannelDataView::SendAllChannelUpdates()
{
	if (Connection == nullptr)
		return 0;

	// Use the Arena for faster allocation. See https://developers.google.com/protocol-buffers/docs/reference/arenas
	google::protobuf::Arena Arena;
	int32 TotalUpdateCount = 0;
	for (auto& Pair : Connection->SubscribedChannels)
	{
		if (static_cast<channeldpb::ChannelDataAccess>(Pair.Value.SubOptions.DataAccess) == channeldpb::WRITE_ACCESS)
		{
			ChannelId ChId = Pair.Key;
			TSet<FProviderInternal>* Providers = ChannelDataProviders.Find(ChId);
			if (Providers == nullptr)
				continue;

			auto MsgTemplate = ChannelDataTemplates.FindRef(static_cast<channeldpb::ChannelType>(Pair.Value.ChannelType));
			if (!ensureMsgf(MsgTemplate, TEXT("Can't find channel data message template of channel type: %d"), Pair.Value.ChannelType))
			{
				continue;
			}

			auto NewState = MsgTemplate->New(&Arena);

			int UpdateCount = 0;
			int RemovedCount = 0;
			for (auto Itr = Providers->CreateIterator(); Itr; ++Itr)
			{
				auto Provider = Itr.ElementIt->Value;
				if (Provider.IsValid())
				{
					if (Provider->UpdateChannelData(NewState))
					{
						UpdateCount++;
					}
					if (Provider->IsRemoved())
					{
						Itr.RemoveCurrent();
						RemovedCount++;
					}
				}
				else
				{
					Itr.RemoveCurrent();
					RemovedCount++;
				}
			}
			if (RemovedCount > 0)
				UE_LOG(LogChanneld, Log, TEXT("Removed %d channel data provider(s) from channel %d"), RemovedCount, ChId);

			if (UpdateCount > 0 || RemovedCount > 0)
			{
				// Merge removed states
				google::protobuf::Message* RemovedData;
				if (RemovedProvidersData.RemoveAndCopyValue(ChId, RemovedData))
				{
					NewState->MergeFrom(*RemovedData);
					delete RemovedData;
				}
				
				channeldpb::ChannelDataUpdateMessage UpdateMsg;
				UpdateMsg.mutable_data()->PackFrom(*NewState);
				Connection->Send(ChId, channeldpb::CHANNEL_DATA_UPDATE, UpdateMsg);

				UE_LOG(LogChanneld, Verbose, TEXT("Sent %s update: %s"), UTF8_TO_TCHAR(NewState->GetTypeName().c_str()), UTF8_TO_TCHAR(NewState->DebugString().c_str()));
			}

			TotalUpdateCount += UpdateCount;
		}
	}

	UMetrics* Metrics = GEngine->GetEngineSubsystem<UMetrics>();
	Metrics->AddConnTypeLabel(*Metrics->ReplicatedProviders).Increment(TotalUpdateCount);

	return TotalUpdateCount;
}

UChanneldGameInstanceSubsystem* UChannelDataView::GetChanneldSubsystem() const
{
	// The subsystem owns the view.
	return Cast<UChanneldGameInstanceSubsystem>(GetOuter());
/*
	UWorld* World = GetWorld();
	// The client may still be in pending net game
	if (World == nullptr)
	{
		// The subsystem owns the view.
		return Cast<UChanneldGameInstanceSubsystem>(GetOuter());
	}
	if (World)
	{
		UGameInstance* GameInstance = World->GetGameInstance();
		if (GameInstance)
		{
			auto Result = GameInstance->GetSubsystem<UChanneldGameInstanceSubsystem>();
			//UE_LOG(LogChanneld, Log, TEXT("Found ChanneldGameInstanceSubsystem: %d"), Result == nullptr ? 0 : 1);
			return Result;
		}
	}
	return nullptr;
*/
}

UObject* UChannelDataView::GetObjectFromNetGUID(const FNetworkGUID& NetId)
{
	if (auto NetDriver = GetChanneldSubsystem()->GetNetDriver())
	{
		return NetDriver->GuidCache->GetObjectFromNetGUID(NetId, true);
	}
	return nullptr;
}

void UChannelDataView::HandleUnsub(UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto UnsubMsg = static_cast<const channeldpb::UnsubscribedFromChannelResultMessage*>(Msg);
	UE_LOG(LogChanneld, Log, TEXT("Received unsub of conn(%d), connType=%s, channelType=%s, channelId=%d"),
		UnsubMsg->connid(),
		UTF8_TO_TCHAR(channeldpb::ConnectionType_Name(UnsubMsg->conntype()).c_str()),
		UTF8_TO_TCHAR(channeldpb::ChannelType_Name(UnsubMsg->channeltype()).c_str()),
		ChId);

	// When current connection unsubs from a channel, remove all providers in that channel.
	if (UnsubMsg->connid() == Connection->GetConnId())
	{
		TSet<FProviderInternal> Providers;
		if (ChannelDataProviders.RemoveAndCopyValue(ChId, Providers))
		{
			UE_LOG(LogChanneld, Log, TEXT("Received Unsub message. Removed all data providers(%d) from channel %d"), Providers.Num(), ChId);
			OnUnsubFromChannel(ChId, Providers);
		}
	}

	if (Connection->IsServer())
	{
		if (UnsubMsg->conntype() == channeldpb::CLIENT && Connection->OwnedChannels.Contains(ChId))
		{
			OnClientUnsub(UnsubMsg->connid(), UnsubMsg->channeltype(), ChId);
		}
	}
}

void UChannelDataView::OnClientUnsub(ConnectionId ClientConnId, channeldpb::ChannelType ChannelType, ChannelId ChId)
{
	if (auto NetDriver = GetChanneldSubsystem()->GetNetDriver())
	{
		if (auto ClientConnection = NetDriver->GetClientConnection(ClientConnId))
		{
			//~ Start copy from UNetDriver::Shutdown()
			if (ClientConnection->PlayerController)
			{
				APawn* Pawn = ClientConnection->PlayerController->GetPawn();
				if (Pawn)
				{
					Pawn->Destroy(true);
				}
			}

			// Calls Close() internally and removes from ClientConnections
			// Will also destroy the player controller.
			ClientConnection->CleanUp();
			//~ End copy
		}
	}
}

void UChannelDataView::HandleChannelDataUpdate(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto UpdateMsg = static_cast<const channeldpb::ChannelDataUpdateMessage*>(Msg);

	google::protobuf::Message* UpdateData;
	if (ReceivedUpdateDataInChannels.Contains(ChId))
	{
		UpdateData = ReceivedUpdateDataInChannels[ChId];
	}
	else
	{
		FString TypeUrl(UTF8_TO_TCHAR(UpdateMsg->data().type_url().c_str()));
		auto MsgTemplate = ChannelDataTemplatesByTypeUrl.FindRef(TypeUrl);
		if (MsgTemplate == nullptr)
		{
			UE_LOG(LogChanneld, Error, TEXT("Unable to find channel data parser by typeUrl: %s"), *TypeUrl);
			return;
		}
		UpdateData = MsgTemplate->New();
	}

	// Call ParsePartial instead of Parse to keep the existing value from being reset.
	if (!UpdateData->ParsePartialFromString(UpdateMsg->data().value()))
	{
		UE_LOG(LogChanneld, Error, TEXT("Failed to parse %s channel data, typeUrl: %s"), *GetChanneldSubsystem()->GetChannelTypeNameByChId(ChId), UTF8_TO_TCHAR(UpdateMsg->data().type_url().c_str()));
		return;
	}

	UE_LOG(LogChanneld, Verbose, TEXT("Received %s channel %d update: %s"), *GetChanneldSubsystem()->GetChannelTypeNameByChId(ChId), ChId, UTF8_TO_TCHAR(UpdateMsg->DebugString().c_str()));

	TSet<FProviderInternal>* Providers = ChannelDataProviders.Find(ChId);
	if (Providers == nullptr || Providers->Num() == 0)
	{
		UE_LOG(LogChanneld, Warning, TEXT("No provider registered for channel %d, typeUrl: %s"), ChId, UTF8_TO_TCHAR(UpdateMsg->data().type_url().c_str()));
		return;
	}

	// The set can be changed during the iteration, when a new provider is created from the UnrealObjectRef during any replicator's OnStateChanged(),
	// or the provider's owner actor got destroyed by removed=true. So we use a const array to iterate.
	TArray<FProviderInternal> ProvidersArr = Providers->Array();
	bool bConsumed = false;
	for (FProviderInternal& Provider : ProvidersArr)
	{
		if (Provider.IsValid() && !Provider->IsRemoved())
		{
			Provider->OnChannelDataUpdated(UpdateData);
			bConsumed = true;
		}
	}

	// All new states are consumed, now we reset the cached object for next parse.
	if (bConsumed)
	{
		UpdateData->Clear();
	}
}

