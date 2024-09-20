#include "ChannelDataView.h"
#include "ChanneldGameInstanceSubsystem.h"
#include "ChanneldNetDriver.h"
#include "ChanneldUtils.h"
#include "EngineUtils.h"
#include "ChanneldMetrics.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/reflection.h"
#include "Replication/ChanneldReplication.h"
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
		RegisterChannelDataTemplate(static_cast<int>(ChannelType), Msg);
	}
	else
	{
		UE_LOG(LogChanneld, Error, TEXT("Failed to register channel data type by name: %s"), *MessageFullName);
	}
}

void UChannelDataView::Initialize(UChanneldConnection* InConn, bool bShouldRecover)
{
	if (Connection == nullptr)
	{
		Connection = InConn;

		LoadCmdLineArgs();

		Connection->AddMessageHandler(channeldpb::CHANNEL_DATA_UPDATE, this, &UChannelDataView::HandleChannelDataUpdate);
		Connection->AddMessageHandler(channeldpb::UNSUB_FROM_CHANNEL, this, &UChannelDataView::HandleUnsub);

		if (bShouldRecover)
		{
			Connection->OnRecoverChannelData.AddUObject(this, &UChannelDataView::RecoverChannelData);
		}
	}

	const auto Settings = GetMutableDefault<UChanneldSettings>();

	for (auto& Pair : Settings->DefaultChannelDataMsgNames)
	{
		RegisterChannelDataType(Pair.Key, Pair.Value);
	}

	if (Connection->IsServer())
	{
		const float InitDelay = Settings->DelayViewInitInSeconds;
		if (InitDelay > 0)
		{
			FTimerHandle Handle;
			GetWorld()->GetTimerManager().SetTimer(Handle, [&](){InitServer(bShouldRecover);}, 1, false, InitDelay);
		}
		else
		{
			InitServer(bShouldRecover);
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
	
	GetChanneldSubsystem()->OnViewInitialized.Broadcast(this);
}

void UChannelDataView::InitServer(bool bShouldRecover)
{
	NetConnForSpawn = NewObject<UChanneldNetConnection>(GetTransientPackage(), UChanneldNetConnection::StaticClass());
	auto NetDriver = GetChanneldSubsystem()->GetNetDriver();
	NetConnForSpawn->InitBase(NetDriver, NetDriver->GetSocket(), FURL(), USOCK_Open);
	ChanneldUtils::InitNetConnForSpawn(NetConnForSpawn);

	auto GameState = GetWorld()->GetAuthGameMode()->GameState;
	auto WorldSettings = GetWorld()->GetWorldSettings();
	if (!bIsGlobalStatesOwner)
	{
		GameState->SetRole(ENetRole::ROLE_SimulatedProxy);
		WorldSettings->SetRole(ENetRole::ROLE_SimulatedProxy);
		SetOwningChannelId(GetNetId(GameState), Channeld::GlobalChannelId);
		SetOwningChannelId(GetNetId(WorldSettings), Channeld::GlobalChannelId);
	}
	// Add the GameStateBase (if it's an IChannelDataProvider).
	// Missing this step will cause client failing to begin play.
	AddObjectProvider(Channeld::GlobalChannelId, GameState);
	AddObjectProvider(Channeld::GlobalChannelId, WorldSettings);
	
	ReceiveInitServer(bShouldRecover);
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

void UChannelDataView::AddProvider(Channeld::ChannelId ChId, IChannelDataProvider* Provider)
{
	/*
	ensureMsgf(Provider->GetChannelType() != channeldpb::UNKNOWN, TEXT("Invalid channel type of data provider: %s"), *IChannelDataProvider::GetName(Provider));
	if (!ChannelDataTemplates.Contains(Provider->GetChannelType()))
	{
		RegisterChannelDataTemplate(Provider->GetChannelType(), Provider->GetChannelDataTemplate());
	}
	*/

	// Make sure provider is not set as removed when adding to a channel.
	Provider->SetRemoved(false);

	TSet<FProviderInternal>& Providers = ChannelDataProviders.FindOrAdd(ChId);
	if (Providers.Contains(Provider))
	{
		UE_LOG(LogChanneld, Verbose, TEXT("Channel data provider already exists in channel %u: %s"), ChId, *IChannelDataProvider::GetName(Provider));
		return;
	}
	
	Providers.Add(Provider);
	ChannelDataProviders[ChId] = Providers;
	UE_LOG(LogChanneld, Verbose, TEXT("Added channel data provider %s to channel %u"), *IChannelDataProvider::GetName(Provider), ChId);
	
	Provider->OnAddedToChannel(ChId);

	if (auto SavedUpdates = UnprocessedUpdateDataInChannels.Find(ChId))
	{
		UE_LOG(LogChanneld, Verbose, TEXT("Consuming %d saved ChannelDataUpdate(s) in channel %u"), SavedUpdates->Num(), ChId);

		for (auto Itr = SavedUpdates->CreateIterator(); Itr; ++Itr)
		{
			auto UpdateData = *Itr;
			Provider->OnChannelDataUpdated(UpdateData);
			Itr.RemoveCurrent();
			delete UpdateData;
		}
	}
}

void UChannelDataView::AddProviderToDefaultChannel(IChannelDataProvider* Provider)
{
	FNetworkGUID NetId = GetNetId(Provider);
	
	if (NetId.IsValid())
	{
		Channeld::ChannelId ChId = GetOwningChannelId(NetId);
		if (ChId != Channeld::InvalidChannelId)
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

bool UChannelDataView::IsObjectProvider(UObject* Obj)
{
	if (Obj == nullptr)
		return false;
	
	if (Obj->Implements<UChannelDataProvider>())
	{
		return true;
	}
	if (AActor* Actor = Cast<AActor>(Obj))
	{
		return Actor->GetComponentsByInterface(UChannelDataProvider::StaticClass()).Num() > 0;
	}
	
	return false;
}

void UChannelDataView::AddObjectProvider(Channeld::ChannelId ChId, UObject* Obj)
{
	if (Obj == nullptr)
		return;
	
	if (Obj->Implements<UChannelDataProvider>())
	{
		AddProvider(ChId, Cast<IChannelDataProvider>(Obj));
	}
	if (AActor* Actor = Cast<AActor>(Obj))
	{
		for (auto Comp : Actor->GetComponentsByInterface(UChannelDataProvider::StaticClass()))
		{
			AddProvider(ChId, Cast<IChannelDataProvider>(Comp));
		}
	}
}

void UChannelDataView::AddObjectProviderToDefaultChannel(UObject* Obj)
{
	if (Obj->Implements<UChannelDataProvider>())
	{
		AddProviderToDefaultChannel(Cast<IChannelDataProvider>(Obj));
	}
	if (AActor* Actor = Cast<AActor>(Obj))
	{
		for (const auto Comp : Actor->GetComponentsByInterface(UChannelDataProvider::StaticClass()))
		{
			AddProviderToDefaultChannel(Cast<IChannelDataProvider>(Comp));
		}
	}
}

void UChannelDataView::RemoveObjectProvider(Channeld::ChannelId ChId, UObject* Obj, bool bSendRemoved)
{
	if (Obj->Implements<UChannelDataProvider>())
	{
		RemoveProvider(ChId, Cast<IChannelDataProvider>(Obj), bSendRemoved);
	}
	if (AActor* Actor = Cast<AActor>(Obj))
	{
		for (const auto Comp : Actor->GetComponentsByInterface(UChannelDataProvider::StaticClass()))
		{
			RemoveProvider(ChId, Cast<IChannelDataProvider>(Comp), bSendRemoved);
		}
	}
}

void UChannelDataView::RemoveObjectProviderAll(UObject* Obj, bool bSendRemoved)
{
	if (Obj->Implements<UChannelDataProvider>())
	{
		RemoveProviderFromAllChannels(Cast<IChannelDataProvider>(Obj), bSendRemoved);
	}
	if (AActor* Actor = Cast<AActor>(Obj))
	{
		for (const auto Comp : Actor->GetComponentsByInterface(UChannelDataProvider::StaticClass()))
		{
			RemoveProviderFromAllChannels(Cast<IChannelDataProvider>(Comp), bSendRemoved);
		}
	}
}

void UChannelDataView::RemoveProvider(Channeld::ChannelId ChId, IChannelDataProvider* Provider, bool bSendRemoved)
{
	if (Provider->IsRemoved())
	{
		return;
	}
	
	Provider->SetRemoved(true);
	
	TSet<FProviderInternal>* Providers = ChannelDataProviders.Find(ChId);
	if (Providers != nullptr)
	{
		UE_LOG(LogChanneld, Verbose, TEXT("Removing channel data provider %s from channel %u"), *IChannelDataProvider::GetName(Provider), ChId);

		const auto ChannelInfo = Connection->SubscribedChannels.Find(ChId);

		// Don't send removal update to the spatial or entity channel
		if (bSendRemoved && ChannelInfo && ChannelInfo->ShouldSendRemovalUpdate())
		{
			// Collect the removed states immediately (before the provider gets destroyed completely)
			google::protobuf::Message* RemovedData = RemovedProvidersData.FindRef(ChId);
			if (!RemovedData)
			{
				const auto MsgTemplate = ChannelDataTemplates.FindRef(static_cast<int>(ChannelInfo->ChannelType));
				if (!ensureMsgf(MsgTemplate, TEXT("Can't find channel data message template of channel type: %s"), *UEnum::GetValueAsString(ChannelInfo->ChannelType)))
				{
					Providers->Remove(Provider);
					Provider->OnRemovedFromChannel(ChId);
					return;
				}
				RemovedData = MsgTemplate->New();
				RemovedProvidersData.Add(ChId, RemovedData);
			}
			Provider->UpdateChannelData(RemovedData);
		}
		else
		{
			Providers->Remove(Provider);
			Provider->OnRemovedFromChannel(ChId);
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

void UChannelDataView::MoveProvider(Channeld::ChannelId OldChId, Channeld::ChannelId NewChId, IChannelDataProvider* Provider, bool bSendRemoved)
{
	RemoveProvider(OldChId, Provider, bSendRemoved);
	if (!Connection->SubscribedChannels.Contains(NewChId))
	{
		UE_LOG(LogChanneld, Warning, TEXT("Moving a provider '%s' to channel %u which hasn't been subscribed yet."), *IChannelDataProvider::GetName(Provider), NewChId);
	}
	AddProvider(NewChId, Provider);
}

void UChannelDataView::MoveObjectProvider(Channeld::ChannelId OldChId, Channeld::ChannelId NewChId, UObject* Provider, bool bSendRemoved)
{
	if (Provider->Implements<UChannelDataProvider>())
	{
		MoveProvider(OldChId, NewChId, Cast<IChannelDataProvider>(Provider), bSendRemoved);
	}
	if (AActor* Actor = Cast<AActor>(Provider))
	{
		for (const auto Comp : Actor->GetComponentsByInterface(UChannelDataProvider::StaticClass()))
		{
			MoveProvider(OldChId, NewChId, Cast<IChannelDataProvider>(Comp), bSendRemoved);
		}
	}
}

UChanneldNetConnection* UChannelDataView::CreateClientConnection(Channeld::ConnectionId ConnId, Channeld::ChannelId ChId)
{
	if (auto NetDriver = GetChanneldSubsystem()->GetNetDriver())//NetDriver.IsValid())
	{
		if (NetDriver->GetClientConnectionMap().Contains(ConnId))
		{
			return NetDriver->GetClientConnectionMap()[ConnId];
		}
		UChanneldNetConnection* ClientConn = NetDriver->AddChanneldClientConnection(ConnId, ChId);
		// Create the ControlChannel and set OpenAcked = 1
		UChannel* ControlChannel = ClientConn->CreateChannelByName(NAME_Control, EChannelCreateFlags::OpenedLocally);
		ControlChannel->OpenAcked = 1;
		ControlChannel->OpenPacketId.First = 0;
		ControlChannel->OpenPacketId.Last = 0;
		
		return ClientConn;
	}
	
	UE_LOG(LogChanneld, Error, TEXT("[Server] Failed to create client connection %d: NetDriver is not valid."), ConnId)
	return nullptr;
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
	SendSpawnToConn(GameMode->GetWorldSettings(), NewPlayerConn, 0);
	
	/* Unfortunately, a couple of RPC on the PC is called in GameMode::PostLogin BEFORE invoking this event. So we need to handle the RPC properly.
	 * UPDATE: no need to worry - the client can queue unmapped RPC now!
	 */
	// Send the PC to the owning client after the PC and NetConn are mutually referenced.
	SendSpawnToConn(NewPlayer, NewPlayerConn, NewPlayerConn->GetConnId());
	SendSpawnToConn(NewPlayer->PlayerState, NewPlayerConn, NewPlayerConn->GetConnId());
	
	/* OnServerSpawnedActor() sends the spawning of the new player's pawn to other clients */

	/*
	// Send the existing player pawns to the new player.
	// PC only exists in owning client as an AutonomousProxy, so we don't send it.
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PC = Iterator->Get();
		if (PC && PC != NewPlayer)
		{
			if (PC->GetPawn())
			{
				NewPlayerConn->SendSpawnMessage(PC->GetPawn(), ENetRole::ROLE_SimulatedProxy);
			}
		}
	}
	*/

	SendExistingActorsToNewPlayer(NewPlayer, NewPlayerConn);
}

void UChannelDataView::SendExistingActorsToNewPlayer(APlayerController* NewPlayer, UChanneldNetConnection* NewPlayerConn)
{
	if (auto NetDriver = GetChanneldSubsystem()->GetNetDriver())
	{
		for(TActorIterator<AActor> It(GetWorld(), AActor::StaticClass()); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor->IsA<AGameModeBase>() && Actor != NewPlayer && Actor != NewPlayer->PlayerState)
			{
				NetDriver->OnServerSpawnedActor(Actor);
			}
		}
	}
}

bool UChannelDataView::CheckUnspawnedObject(Channeld::ChannelId ChId, google::protobuf::Message* ChannelData)
{
	// Only client or recovering server needs to spawn the objects.
	if (Connection->IsServer() && !Connection->IsRecovering())
	{
		return false;
	}

	EChanneldChannelType ChannelType = GetChanneldSubsystem()->GetChannelTypeByChId(ChId);
	if (ChannelType == EChanneldChannelType::ECT_Global || ChannelType == EChanneldChannelType::ECT_SubWorld)
	{
		/*
		// Key: NetId, Value: <Key: class of the state, Value: owningConnId of the state actor>
		TMap<uint32, TTuple<UClass*, uint32>> UnresolvedStates;
		
		for (int i = 0; i < ChannelData->GetDescriptor()->field_count(); i++)
		{
			auto Field = ChannelData->GetDescriptor()->field(i);
			if (Field->is_map())
			{
				auto KeysField = Field->message_type()->map_key();
				auto ValuesField = Field->message_type()->map_value();
				auto RepeatedStates = ChannelData->GetReflection()->GetRepeatedFieldRef<google::protobuf::Message>(*ChannelData, Field);
				int StatesNum = RepeatedStates.size();
				for (int j = 0; j < StatesNum; j++)
				{
					auto& KV = RepeatedStates.Get(j, ChannelData);
					uint32 NetId = KV.GetReflection()->GetUInt32(KV, KeysField);
					if (HasEverSpawned(NetId))
					{
						continue;
					}
					auto StateMessage = &KV.GetReflection()->GetMessage(KV, ValuesField);
					UClass* StateClass = LoadClassFromProto(StateMessage->GetDescriptor());
					auto& Tuple = UnresolvedStates.FindOrAdd(NetId);
					if (Tuple.Key == nullptr || StateClass->IsChildOf(Tuple.Key))
					{
						Tuple.Key = StateClass;
					}
					if (StateMessage->GetDescriptor()->name() == "ActorState")
					{
						auto ActorState = static_cast<const unrealpb::ActorState*>(StateMessage);
						Tuple.Value = ActorState->owningconnid();
					}
					// if (CheckUnspawnedChannelDataState(ChId, StateMessage->GetDescriptor(), NetId))
					// {
					// 	return true;
					// }
				}
			}
			// Singleton state
			else
			{
				// return CheckUnspawnedChannelDataState(ChId, Field->message_type());
				UClass* StateClass = LoadClassFromProto(Field->message_type());
				if (StateClass == nullptr)
				{
					continue;
				}
				uint32 NetId = FStaticGuidRegistry::GetStaticObjectExportedNetGUID(*StateClass->GetPathName());
				if (NetId == 0)
				{
					UE_LOG(LogChanneld, Warning, TEXT("Proto field '%s' is not in FStaticGuidRegistry, channel id: %u"), UTF8_TO_TCHAR(Field->full_name().c_str()), ChId);
					continue;
				}
				if (HasEverSpawned(NetId))
				{
					continue;
				}
				auto& Tuple = UnresolvedStates.FindOrAdd(NetId);
				if (Tuple.Key == nullptr || StateClass->IsChildOf(Tuple.Key))
				{
					Tuple.Key = StateClass;
				}
			}
		}

		// Sort the unresolved states by NetId
		UnresolvedStates.KeySort([](uint32 A, uint32 B) { return A < B; });
		
		for (auto Pair : UnresolvedStates)
		{
			uint32 NetId = Pair.Key;
			UClass* StateClass = Pair.Value.Key;
			uint32 OwningConnId = Pair.Value.Value;
			UChanneldNetConnection* OwningConn = nullptr;
			if (OwningConnId > 0 && Connection->IsServer())
			{
				OwningConn = GetChanneldSubsystem()->GetNetDriver()->GetClientConnectionMap().FindRef(OwningConnId);
				if (OwningConn == nullptr)
				{
					OwningConn = CreateClientConnection(OwningConnId, ChId);
					UE_LOG(LogChanneld, Log, TEXT("[Server] Created NetConnection %u for %s, NetId: %u"), OwningConnId, *StateClass->GetName(), NetId);
				}
			}
			FString ClassPath = StateClass->GetPathName();
			unrealpb::UnrealObjectRef ObjRef;
			ObjRef.set_netguid(NetId);
			ObjRef.set_classpath(TCHAR_TO_UTF8(*ClassPath));
			ObjRef.set_owningconnid(OwningConnId);

			UE_LOG(LogChanneld, Verbose, TEXT("Spawning object from unresolved channel data state, ClassPath: %s, NetId: %u"), *ClassPath, ObjRef.netguid());
			UObject* NewObj = ChanneldUtils::GetObjectByRef(&ObjRef, GetWorld(), true, OwningConn);
			if (NewObj)
			{
				AddObjectProvider(ChId, NewObj);

				if (Connection->IsServer() && NewObj->IsA<APlayerController>() && OwningConn != nullptr)
				{
					Cast<APlayerController>(NewObj)->NetConnection = OwningConn;
				}
			}
		}
		*/
	}
	else if (ChannelType == EChanneldChannelType::ECT_Entity)
	{
		if (HasEverSpawned(ChId))
		{
			return false;
		}
		
		auto ObjRefField = ChannelData->GetDescriptor()->field(0);
		if (ObjRefField == nullptr)
		{
			return true;
		}
		if (!ensureMsgf(ObjRefField->name() == "objRef", TEXT("EntityChannelData's first field should be 'objRef' but is '%s'"), UTF8_TO_TCHAR(ObjRefField->name().c_str())))
		{
			return true;
		}
		if (!ChannelData->GetReflection()->HasField(*ChannelData, ObjRefField))
		{
			return true;
		}
	
		auto& ObjRef = static_cast<const unrealpb::UnrealObjectRef&>(ChannelData->GetReflection()->GetMessage(*ChannelData, ObjRefField));
		TCHAR* ClassPath = UTF8_TO_TCHAR(ObjRef.classpath().c_str());
		if (UClass* EntityClass = LoadObject<UClass>(nullptr, ClassPath))
		{
			// Do not resolve other PlayerController or PlayerState on the client.
			if (Connection->IsClient() && (EntityClass->IsChildOf(APlayerController::StaticClass()) || EntityClass->IsChildOf(APlayerState::StaticClass())))
			{
				return true;
			}
		}

		UE_LOG(LogChanneld, Verbose, TEXT("[Client] Spawning object from unresolved EntityChannelData, NetId: %u"), ObjRef.netguid());
		UObject* NewObj = ChanneldUtils::GetObjectByRef(&ObjRef, GetWorld());
		if (NewObj)
		{
			AddObjectProvider(ChId, NewObj);
			/* We don't know the spatial channel id of the entity yet.
			OnNetSpawnedObject(NewObj, ChId);
			*/
		}
	}
	else if (ChannelType == EChanneldChannelType::ECT_Spatial)
	{
		auto SpatialChannelData = static_cast<const unrealpb::SpatialChannelData*>(ChannelData);
		for (auto& Pair : SpatialChannelData->entities())
		{
			if (HasEverSpawned(Pair.first))
			{
				continue;
			}

			auto& ObjRef = Pair.second.objref();
			TCHAR* ClassPath = UTF8_TO_TCHAR(ObjRef.classpath().c_str());
			if (UClass* EntityClass = LoadObject<UClass>(nullptr, ClassPath))
			{
				// Do not resolve other PlayerController on the client.
				if (EntityClass->IsChildOf(APlayerController::StaticClass()) || EntityClass->IsChildOf(APlayerState::StaticClass()))
				{
					continue;
				}
			}

			// Set up the mapping before actually spawn it, so AddProvider() can find the mapping.
			SetOwningChannelId(ObjRef.netguid(), ChId);
			
			// Also add the mapping of all context NetGUIDs
			for (auto& ContextObj : ObjRef.context())
			{
				SetOwningChannelId(ContextObj.netguid(), ChId);
			}

			UE_LOG(LogChanneld, Verbose, TEXT("[Client] Spawning object from unresolved SpatialEntityState, NetId: %u"), ObjRef.netguid());
			UObject* NewObj = ChanneldUtils::GetObjectByRef(&ObjRef, GetWorld());
			if (NewObj)
			{
				AddObjectProviderToDefaultChannel(NewObj);
				OnNetSpawnedObject(NewObj, ChId);
			}
		}
	}
	
	return false;
}

bool UChannelDataView::CheckUnspawnedChannelDataState(Channeld::ChannelId ChId, const google::protobuf::Descriptor* Descriptor, uint32 NetId)
{
	google::protobuf::MessageOptions MessageOptions = Descriptor->options();
	if (!MessageOptions.HasExtension(unrealpb::unreal_class_path))
	{
		UE_LOG(LogChanneld, Warning, TEXT("Proto message '%s' has no 'unreal_class_path' option, channel id: %u"), UTF8_TO_TCHAR(Descriptor->full_name().c_str()), ChId);
		return false;
	}
	std::string strClassPath = MessageOptions.GetExtension(unrealpb::unreal_class_path);
	if (strClassPath.length() == 0)
	{
		UE_LOG(LogChanneld, Warning, TEXT("Proto message '%s' has no 'unreal_class_path' option, channel id: %u"), UTF8_TO_TCHAR(Descriptor->full_name().c_str()), ChId);
		return false;
	}
	TCHAR* ClassPath = UTF8_TO_TCHAR(strClassPath.c_str());
	UClass* ObjClass = LoadObject<UClass>(nullptr, ClassPath);
	if (ObjClass == nullptr)
	{
		UE_LOG(LogChanneld, Warning, TEXT("Failed to load class from unreal_class_path: %s, proto field: %s, channel id: %u"), *ClassPath, UTF8_TO_TCHAR(Descriptor->full_name().c_str()), ChId);
		return false;
	}
	
	// Do not resolve other PlayerController or PlayerState on the client.
	if (Connection->IsClient() && (ObjClass->IsChildOf(APlayerController::StaticClass()) || ObjClass->IsChildOf(APlayerState::StaticClass())))
	{
		return true;
	}
	
	if (NetId == 0)
	{
		NetId = FStaticGuidRegistry::GetStaticObjectExportedNetGUID(ClassPath);
	}
	if (NetId == 0)
	{
		UE_LOG(LogChanneld, Warning, TEXT("Proto field '%s' is not in FStaticGuidRegistry, channel id: %u"), UTF8_TO_TCHAR(Descriptor->full_name().c_str()), ChId);
		return false;
	}

	if (HasEverSpawned(NetId))
	{
		return false;
	}
	
	unrealpb::UnrealObjectRef ObjRef;
	ObjRef.set_netguid(NetId);
	ObjRef.set_classpath(strClassPath);

	UE_LOG(LogChanneld, Verbose, TEXT("[Client] Spawning object from unresolved proto field '%s', NetId: %u"), UTF8_TO_TCHAR(Descriptor->full_name().c_str()), ObjRef.netguid());
	UObject* NewObj = ChanneldUtils::GetObjectByRef(&ObjRef, GetWorld());
	if (NewObj)
	{
		AddObjectProvider(ChId, NewObj);

		if (Connection->IsServer() && NewObj->IsA<APlayerController>())
		{
			
		}
	}
	return false;
}

UClass* UChannelDataView::LoadClassFromProto(const google::protobuf::Descriptor* Descriptor)
{
	google::protobuf::MessageOptions MessageOptions = Descriptor->options();
	if (!MessageOptions.HasExtension(unrealpb::unreal_class_path))
	{
		UE_LOG(LogChanneld, Warning, TEXT("Proto message '%s' has no 'unreal_class_path' option"), UTF8_TO_TCHAR(Descriptor->full_name().c_str()));
		return nullptr;
	}
	std::string strClassPath = MessageOptions.GetExtension(unrealpb::unreal_class_path);
	if (strClassPath.length() == 0)
	{
		UE_LOG(LogChanneld, Warning, TEXT("Proto message '%s' has no 'unreal_class_path' option"), UTF8_TO_TCHAR(Descriptor->full_name().c_str()));
		return nullptr;
	}
	TCHAR* ClassPath = UTF8_TO_TCHAR(strClassPath.c_str());
	UClass* Class = LoadObject<UClass>(nullptr, ClassPath);
	if (Class == nullptr)
	{
		UE_LOG(LogChanneld, Warning, TEXT("Failed to load class from unreal_class_path: %s, proto field: %s"), *ClassPath, UTF8_TO_TCHAR(Descriptor->full_name().c_str()));
		return nullptr;
	}
	return Class;
}

bool UChannelDataView::HasEverSpawned(uint32 NetId) const
{
	auto NetDriver = GetChanneldSubsystem()->GetNetDriver();
	if (!NetDriver)
	{
		return false;
	}

	FNetworkGUID NetGUID(NetId);
	// Don't use IsGUIDRegistered - the object may still exist in GuidCache but has been deleted.
	if (auto CacheObj = NetDriver->GuidCache->ObjectLookup.Find(NetGUID))
	{
		// Already spawned
		if (CacheObj->Object.IsValid())
		{
			return true;
		}
	}
	return false;
}

void UChannelDataView::RecoverChannelData(Channeld::ChannelId ChId, TSharedPtr<channeldpb::ChannelDataRecoveryMessage> RecoveryMsg)
{
	unrealpb::ChannelRecoveryData RecoveryData;
	if (!RecoveryMsg->recoverydata().UnpackTo(&RecoveryData))
	{
		UE_LOG(LogChanneld, Error, TEXT("Failed to unpack ChannelRecoveryData from ChannelDataRecoveryMessage, channel id: %u"), ChId);
		return;
	}

	TMap<uint32, unrealpb::UnrealObjectRef> ObjRefs;
	for (auto Pair : RecoveryData.objrefs())
	{
		ObjRefs.Emplace(Pair.first, Pair.second);
	}
	// Sort the ObjRefs by NetId to create the objects in the correct order.
	ObjRefs.KeySort([](uint32 A, uint32 B) { return A < B; });

	/*
	*/
	for (auto& Pair : ObjRefs)
	{
		FNetworkGUID NetId = FNetworkGUID(Pair.Key);
		FString ClassPath = UTF8_TO_TCHAR(Pair.Value.classpath().c_str());
		uint32 OwningConnId = Pair.Value.owningconnid();
		UChanneldNetConnection* OwningConn = nullptr;
		if (OwningConnId > 0 && Connection->IsServer())
		{
			OwningConn = GetChanneldSubsystem()->GetNetDriver()->GetClientConnectionMap().FindRef(OwningConnId);
			if (OwningConn == nullptr)
			{
				OwningConn = CreateClientConnection(OwningConnId, ChId);
				UE_LOG(LogChanneld, Log, TEXT("[Server] Created NetConnection %u for %s, NetId: %u"), OwningConnId, *ClassPath, NetId.Value);
			}
		}

		UE_LOG(LogChanneld, Verbose, TEXT("Recovering object in channel %u, ClassPath: %s, NetId: %u"), ChId, *ClassPath, NetId.Value);
		UObject* NewObj = ChanneldUtils::GetObjectByRef(&Pair.Value, GetWorld(), true, OwningConn);
		if (NewObj)
		{
			SetOwningChannelId(NetId, ChId);
			AddObjectProvider(ChId, NewObj);

			if (Connection->IsServer() && NewObj->IsA<APlayerController>() && OwningConn != nullptr)
			{
				Cast<APlayerController>(NewObj)->NetConnection = OwningConn;
			}
		}
	}
	
	auto UpdateData = ParseAndMergeUpdateData(ChId, RecoveryMsg->channeldata());
	if (UpdateData == nullptr)
	{
		return;
	}

	/*
	if (CheckUnspawnedObject(ChId, UpdateData))
	{
		UE_LOG(LogChanneld, Verbose, TEXT("Resolving unspawned object, the channel data will not be consumed."));
		return;
	}
	*/
	
	ConsumeChannelUpdateData(ChId, UpdateData);
}

FNetworkGUID UChannelDataView::GetNetId(UObject* Obj, bool bAssignOnServer/* = true*/) const
{
	FNetworkGUID NetId;
	if (const auto NetDriver = GetChanneldSubsystem()->GetNetDriver())
	{
		if (NetDriver->IsServer() && bAssignOnServer)
		{
			NetId = NetDriver->GuidCache->GetOrAssignNetGUID(Obj);
		}
		else
		{
			NetId = ChanneldUtils::GetNetId(Obj, NetDriver);
		}
	}
	return NetId;
}

FNetworkGUID UChannelDataView::GetNetId(IChannelDataProvider* Provider) const
{
	return GetNetId(Provider->GetTargetObject());
}

Channeld::ChannelId UChannelDataView::GetDefaultChannelId() const
{
	return GetChanneldSubsystem()->LowLevelSendToChannelId.Get();
}

bool UChannelDataView::OnServerSpawnedObject(UObject* Obj, const FNetworkGUID NetId)
{
	if (!NetId.IsValid())
		return false;

	Channeld::ChannelId ChId = GetDefaultChannelId();
	SetOwningChannelId(NetId, ChId);
	// NetIdOwningChannels.Add(NetId, ChId);
	// UE_LOG(LogChanneld, Log, TEXT("Set up mapping of netId: %u -> channelId: %d, spawned: %s"), NetId.Value, ChId, *GetNameSafe(Obj));

	AddObjectProvider(ChId, Obj);

	/* Send spawn of the static object to client so they can add the channel data provider.
	if (NetId.IsStatic())
	{
		return false;
	}
	*/

	return true;
}

void UChannelDataView::SendSpawnToClients(UObject* Obj, uint32 OwningConnId)
{
	const auto NetDriver = GetChanneldSubsystem()->GetNetDriver();
	if (!NetDriver)
	{
		UE_LOG(LogChanneld, Error, TEXT("UChannelDataView::SendSpawnToClients: Unable to get ChanneldNetDriver"));
		return;
	}
	
	for (auto& Pair : NetDriver->GetClientConnectionMap())
	{
		if (IsValid(Pair.Value))
		{
			SendSpawnToConn(Obj, Pair.Value, OwningConnId);
		}
	}
}

void UChannelDataView::SendDestroyToClients(UObject* Obj, const FNetworkGUID NetId)
{
	// Don't broadcast the destroy of objects that are only spawned in the owning client.
	if (Obj->IsA<APlayerController>())
	{
		return;
	}
	
	const auto NetDriver = GetChanneldSubsystem()->GetNetDriver();
	if (!NetDriver)
	{
		UE_LOG(LogChanneld, Error, TEXT("UChannelDataView::SendDestroyToClients: Unable to get ChanneldNetDriver"));
		return;
	}
	
	for (auto& Pair : NetDriver->GetClientConnectionMap())
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->SendDestroyMessage(Obj);
		}
	}
}

void UChannelDataView::SendSpawnToConn(UObject* Obj, UChanneldNetConnection* NetConn, uint32 OwningConnId)
{
	// Gameplay Debugger is not supported yet.
	if (Obj->GetClass()->GetFName() == Channeld::GameplayerDebuggerClassName)
	{
		return;
	}
	
	ENetRole Role = ROLE_None;
	if (const AActor* Actor = Cast<AActor>(Obj))
	{
		Role = Actor->GetRemoteRole();
	}
	NetConn->SendSpawnMessage(Obj, Role, Channeld::InvalidChannelId, OwningConnId);
}

void UChannelDataView::OnDestroyedActor(AActor* Actor, const FNetworkGUID NetId)
{
	if (!NetId.IsValid())
		return;

 	Channeld::ChannelId RemovedChId = NetIdOwningChannels.Remove(NetId);
	UE_LOG(LogChanneld, Log, TEXT("Removed mapping of netId: %u (%u) -> channelId: %d"), NetId.Value, ChanneldUtils::GetNativeNetId(NetId.Value), RemovedChId);

	RemoveObjectProviderAll(Actor, false);
}

void UChannelDataView::SetOwningChannelId(const FNetworkGUID NetId, Channeld::ChannelId ChId)
{
	if (!NetId.IsValid())
		return;
	
	NetIdOwningChannels.Add(NetId, ChId);
	UE_LOG(LogChanneld, Log, TEXT("Set up mapping of netId: %u (%u) -> channelId: %d"), NetId.Value, ChanneldUtils::GetNativeNetId(NetId.Value), ChId);

	/*
	if (Connection->IsServer())
	{
		if (AActor* Actor = Cast<AActor>(GetObjectFromNetGUID(NetId)))
		{
			if (Connection->OwnedChannels.Contains(ChId))
			{
				Actor->SetRole(ROLE_Authority);
			}
			else
			{
				Actor->SetRole(ROLE_SimulatedProxy);
			}
			UE_LOG(LogChanneld, Verbose, TEXT("[Server] Set %s's NetRole to %s"), *Actor->GetName(), *UEnum::GetValueAsString(TEXT("Engine.ENetRole"), Actor->GetLocalRole()));
		}
	}
	*/
}

Channeld::ChannelId UChannelDataView::GetOwningChannelId(const FNetworkGUID NetId) const
{
	const Channeld::ChannelId* ChId = NetIdOwningChannels.Find(NetId);
	if (ChId)
	{
		return *ChId;
	}
	return Channeld::InvalidChannelId;
}

Channeld::ChannelId UChannelDataView::GetOwningChannelId(UObject* Obj) const
{
	return GetOwningChannelId(GetNetId(Obj));
}

bool UChannelDataView::SendMulticastRPC(AActor* Actor, const FString& FuncName, TSharedPtr<google::protobuf::Message> ParamsMsg, const FString& SubObjectPathName)
{
	unrealpb::RemoteFunctionMessage RpcMsg;
	RpcMsg.mutable_targetobj()->set_netguid(GetNetId(Actor).Value);
	RpcMsg.set_functionname(TCHAR_TO_UTF8(*FuncName), FuncName.Len());
	RpcMsg.set_subobjectpath(TCHAR_TO_UTF8(*SubObjectPathName), SubObjectPathName.Len());
	if (ParamsMsg)
	{
		RpcMsg.set_paramspayload(ParamsMsg->SerializeAsString());
		UE_LOG(LogChanneld, VeryVerbose, TEXT("Serialized RPC parameters to %d bytes"), RpcMsg.paramspayload().size());
	}

	auto ChId = GetOwningChannelId(Actor);
	// Does this server owns the channel that owns the actor?
	if (auto ChannelInfo = Connection->OwnedChannels.Find(ChId))
	{
		if (ChannelInfo->ChannelType == EChanneldChannelType::ECT_Global || ChannelInfo->ChannelType == EChanneldChannelType::ECT_SubWorld)
		{
			Connection->Broadcast(ChId, unrealpb::RPC, RpcMsg, channeldpb::ALL_BUT_SERVER);
		}
		else if (ChannelInfo->ChannelType == EChanneldChannelType::ECT_Spatial)
		{
			Connection->Broadcast(ChId, unrealpb::RPC, RpcMsg, channeldpb::ADJACENT_CHANNELS | channeldpb::ALL_BUT_SERVER);
		}
		else
		{
			UE_LOG(LogChanneld, Warning, TEXT("Multicast RPC is only supported in Global, SubWorld and Spatial channels. ChannelId: %d, ChannelType: %d"), ChId, (int32)ChannelInfo->ChannelType);
			GEngine->GetEngineSubsystem<UChanneldMetrics>()->OnDroppedRPC(RpcMsg.functionname(), RPCDropReason_InvalidMulticast);
			return false;
		}
	}
	else
	{
		// Forward the RPC to the server that owns the channel / actor.
		Connection->Broadcast(ChId, unrealpb::RPC, RpcMsg, channeldpb::SINGLE_CONNECTION);
		UE_LOG(LogChanneld, Log, TEXT("Forwarded RPC %s::%s to the owner of channel %u"), *Actor->GetName(), *FuncName, ChId);
	}

	if (auto NetDriver = GetChanneldSubsystem()->GetNetDriver())
	{
		NetDriver->OnSentRPC(RpcMsg);
	}
	
	return true;
}

void UChannelDataView::OnDisconnect()
{
	for (auto& Pair : ChannelDataProviders)
	{
		for (FProviderInternal& Provider : Pair.Value)
		{
			if (Provider.IsValid())
			{
				Provider->SetRemoved(true);
			}
		}
	}

	// Force to send the channel update data with the removed states to channeld
	SendAllChannelUpdates();
}

int32 UChannelDataView::SendChannelUpdate(Channeld::ChannelId ChId)
{
	auto ChannelInfo = Connection->SubscribedChannels.Find(ChId);
	if (ChannelInfo == nullptr)
	{
		UE_LOG(LogChanneld, Warning, TEXT("Failed to SendChannelUpdate due to no subscription found for channel %u"), ChId);
		return 0;
	}
	if (ChannelInfo->SubOptions.DataAccess != EChannelDataAccess::EDA_WRITE_ACCESS && !Connection->OwnedChannels.Contains(ChId))
	{
		return 0;
	}
	
	TSet<FProviderInternal>* Providers = ChannelDataProviders.Find(ChId);
	if (Providers == nullptr || Providers->Num() == 0)
	{
		return 0;
	}

	auto MsgTemplate = ChannelDataTemplates.FindRef(static_cast<int>(ChannelInfo->ChannelType));
	if (!ensureMsgf(MsgTemplate, TEXT("Can't find channel data message template of channel type: %d"), ChannelInfo->ChannelType))
	{
		return 0;
	}

	auto DeltaChannelData = MsgTemplate->New(&ArenaForSend);

	int UpdateCount = 0;
	int RemovedCount = 0;
	for (auto Itr = Providers->CreateIterator(); Itr; ++Itr)
	{
		auto Provider = Itr.ElementIt->Value;
		if (Provider.IsValid())
		{
			/* Pre-replication logic should be implemented in the replicator.
			Provider->GetTargetObject()->CallPreReplication();
			*/
			if (Provider->UpdateChannelData(DeltaChannelData))
			{
				UpdateCount++;
			}
			if (Provider->IsRemoved())
			{
				Itr.RemoveCurrent();
				RemovedCount++;
				Provider->OnRemovedFromChannel(ChId);
			}
		}
		else
		{
			Itr.RemoveCurrent();
			RemovedCount++;
		}
	}
	if (RemovedCount > 0)
	{
		UE_LOG(LogChanneld, Log, TEXT("Removed %d channel data provider(s) from channel %u"), RemovedCount, ChId);
	}

	if (UpdateCount > 0 || (RemovedCount > 0 && ChannelInfo->ShouldSendRemovalUpdate()))
	{
		// Merge removed states
		google::protobuf::Message* RemovedData;
		if (RemovedProvidersData.RemoveAndCopyValue(ChId, RemovedData))
		{
			DeltaChannelData->MergeFrom(*RemovedData);
			delete RemovedData;
		}
				
		channeldpb::ChannelDataUpdateMessage UpdateMsg;
		UpdateMsg.mutable_data()->PackFrom(*DeltaChannelData);
		Connection->Send(ChId, channeldpb::CHANNEL_DATA_UPDATE, UpdateMsg);

		UE_LOG(LogChanneld, Verbose, TEXT("Sent %s update: %s"), UTF8_TO_TCHAR(DeltaChannelData->GetTypeName().c_str()), UTF8_TO_TCHAR(DeltaChannelData->DebugString().c_str()));
	}

	return UpdateCount;
}

int32 UChannelDataView::SendAllChannelUpdates()
{
	if (Connection == nullptr)
		return 0;

	int32 TotalUpdateCount = 0;
	for (auto& Pair : Connection->SubscribedChannels)
	{
		// Skip sending updates for spatial channels. The channel data is maintained by channeld (by handling the Spawn and Destroy messages)
		if (Pair.Value.ChannelType == EChanneldChannelType::ECT_Spatial)
		{
			continue;
		}
		
		TotalUpdateCount += SendChannelUpdate(Pair.Key);
	}

	ArenaForSend.Reset();

	if (TotalUpdateCount > 0)
	{
		UChanneldMetrics* Metrics = GEngine->GetEngineSubsystem<UChanneldMetrics>();
		Metrics->ReplicatedProviders_Counter->Increment(TotalUpdateCount);
	}

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

UObject* UChannelDataView::GetObjectFromNetGUID(const FNetworkGUID& NetId) const
{
	if (auto NetDriver = GetChanneldSubsystem()->GetNetDriver())
	{
		if (NetId.IsStatic())
		{
			return FStaticGuidRegistry::GetStaticObject(NetId, NetDriver);
		}
	
		return NetDriver->GuidCache->GetObjectFromNetGUID(NetId, true);
	}
	return nullptr;
}

void UChannelDataView::HandleUnsub(UChanneldConnection* _, Channeld::ChannelId ChId, const google::protobuf::Message* Msg)
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
			UE_LOG(LogChanneld, Log, TEXT("Received Unsub message. Removed all data providers(%d) from channel %u"), Providers.Num(), ChId);
			OnRemovedProvidersFromChannel(ChId, UnsubMsg->channeltype(), Providers);
		}
	}

	if (Connection->IsServer())
	{
		if (UnsubMsg->conntype() == channeldpb::CLIENT && Connection->OwnedChannels.Contains(ChId))
		{
			ServerHandleClientUnsub(UnsubMsg->connid(), UnsubMsg->channeltype(), ChId);
		}
	}
}

void UChannelDataView::ServerHandleClientUnsub(Channeld::ConnectionId ClientConnId, channeldpb::ChannelType ChannelType, Channeld::ChannelId ChId)
{
	if (auto NetDriver = GetChanneldSubsystem()->GetNetDriver())
	{
		UE_LOG(LogChanneld, Log, TEXT("Client leaves the game, removing the connection: %d"), ClientConnId);
		NetDriver->RemoveChanneldClientConnection(ClientConnId);
	}
}

google::protobuf::Message* UChannelDataView::ParseAndMergeUpdateData(Channeld::ChannelId ChId, const google::protobuf::Any& AnyData)
{
	FString TypeUrl(UTF8_TO_TCHAR(AnyData.type_url().c_str()));
	auto MsgTemplate = ChannelDataTemplatesByTypeUrl.FindRef(TypeUrl);
	if (MsgTemplate == nullptr)
	{
		UE_LOG(LogChanneld, Error, TEXT("Unable to find channel data template by typeUrl: %s"), *TypeUrl);
		return nullptr;
	}

	google::protobuf::Message* UpdateData;
	if (ReceivedUpdateDataInChannels.Contains(ChId))
	{
		UpdateData = ReceivedUpdateDataInChannels[ChId];
	}
	else
	{
		UpdateData = MsgTemplate->New();
		ReceivedUpdateDataInChannels.Add(ChId, UpdateData);
	}

	FString ChannelTypeName = GetChanneldSubsystem()->GetChannelTypeNameByChId(ChId);
	UE_LOG(LogChanneld, Verbose, TEXT("Received %s channel %u update(%llu B): %s"), *ChannelTypeName, ChId, AnyData.value().size(), *TypeUrl);

	const FName MessageName = UTF8_TO_TCHAR(UpdateData->GetTypeName().c_str());
	auto Processor = ChanneldReplication::FindChannelDataProcessor(MessageName);
	if (Processor)
	{
		// Use the message template as the temporary message to unpack the any data.
		if (!AnyData.UnpackTo(MsgTemplate))
		{
			UE_LOG(LogChanneld, Warning, TEXT("Failed to unpack %s channel data, typeUrl: %s"), *ChannelTypeName, *TypeUrl);
			return nullptr;
		}
		if (!Processor->Merge(MsgTemplate, UpdateData))
		{
			UE_LOG(LogChanneld, Warning, TEXT("Failed to merge %s channel data: %s"), *ChannelTypeName, UTF8_TO_TCHAR(MsgTemplate->DebugString().c_str()));
			return nullptr;
		}
	}
	else
	{
		UE_LOG(LogChanneld, Log, TEXT("ChannelDataProcessor not found for type: %s, fall back to ParsePartialFromString. Risk: The state with the same NetId will be overwritten instead of merged."), *MessageName.ToString());
		// Call ParsePartial instead of Parse to keep the existing value from being reset.
		if (!UpdateData->ParsePartialFromString(AnyData.value()))
		{
			UE_LOG(LogChanneld, Error, TEXT("Failed to parse %s channel data, typeUrl: %s"), *ChannelTypeName, *TypeUrl);
			return nullptr;
		}
	}

	return UpdateData;
}

void UChannelDataView::HandleChannelDataUpdate(UChanneldConnection* Conn, Channeld::ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto UpdateMsg = static_cast<const channeldpb::ChannelDataUpdateMessage*>(Msg);

	auto UpdateData = ParseAndMergeUpdateData(ChId, UpdateMsg->data());
	if (UpdateData == nullptr)
	{
		return;
	}

	if (CheckUnspawnedObject(ChId, UpdateData))
	{
		UE_LOG(LogChanneld, Verbose, TEXT("Resolving unspawned object, the channel data will not be consumed."));
		return;
	}
	
	ConsumeChannelUpdateData(ChId, UpdateData);
}

bool UChannelDataView::ConsumeChannelUpdateData(Channeld::ChannelId ChId, google::protobuf::Message* UpdateData)
{
	TSet<FProviderInternal>* Providers = ChannelDataProviders.Find(ChId);
	if (Providers == nullptr || Providers->Num() == 0)
	{
		UE_LOG(LogChanneld, Log, TEXT("No provider registered for channel %u. The update will not be applied."), ChId);

		/* Saving the update data with removed=true can caused the provider gets destroyed immediately after AddProvider.
		auto UnprocessedUpdateData = UpdateData->New();
		UnprocessedUpdateData->CopyFrom(*UpdateData);
		UnprocessedUpdateDataInChannels.FindOrAdd(ChId).Add(UnprocessedUpdateData);
		*/
		
		return false;
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

	// All new states are consumed, now we reset the cached object for the next parse.
	if (bConsumed)
	{
		UpdateData->Clear();
	}
	
	return bConsumed;
}

/* The following comment no longer applies since we added caching for ChanneldUtils::GetRefOfObject.
// Warning: DO NOT use this function before sending the Spawn message!
// Calling Provider->UpdateChannelData can cause FChanneldActorReplicator::Tick to be called, which will call
// ChanneldUtils::GetRefOfObject and export the NetGUIDs, causing the UnrealObjectRef in the Spawn message
// missing the contexts, so the client will fail to Spawn the object.
*/
const google::protobuf::Message* UChannelDataView::GetEntityData(UObject* Obj)
{
	IChannelDataProvider* Provider= nullptr;
	if (Obj->GetClass()->ImplementsInterface(UChannelDataProvider::StaticClass()))
	{
		Provider = Cast<IChannelDataProvider>(Obj);
	}
	else if (AActor* Actor = Cast<AActor>(Obj))
	{
		auto Comps = Actor->GetComponentsByInterface(UChannelDataProvider::StaticClass());
		if (Comps.Num() > 0)
		{
			Provider = Cast<IChannelDataProvider>(Comps[0]);
		}
	}

	if (Provider == nullptr)
	{
		return nullptr;
	}

	auto MsgTemplate = ChannelDataTemplates.FindRef(channeldpb::ENTITY);
	if (!ensureMsgf(MsgTemplate, TEXT("Can't find channel data message template of entity channel.")))
	{
		return nullptr;
	}

	auto ChannelData = MsgTemplate->New(&ArenaForSend);
	Provider->UpdateChannelData(ChannelData);

	return ChannelData;
}

