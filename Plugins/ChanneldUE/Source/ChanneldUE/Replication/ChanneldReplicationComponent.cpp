#include "ChanneldReplicationComponent.h"
#include "ChanneldGameInstanceSubsystem.h"
#include "ChannelDataProvider.h"
#include "View/ChannelDataView.h"
#include "Engine/ActorChannel.h"
#include "Engine/PackageMapClient.h"
#include "Misc/NetworkGuid.h"
#include "ChanneldReplication.h"
#include "ChanneldNetDriver.h"
#include "ChanneldSettings.h"
#include "GameFramework/GameStateBase.h"

UChanneldReplicationComponent::UChanneldReplicationComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

const google::protobuf::Message* UChanneldReplicationComponent::GetStateFromChannelData(google::protobuf::Message* ChannelData, UClass* TargetClass, uint32 NetGUID, bool& bIsRemoved)
{
	bIsRemoved = false;
	return nullptr;
}

void UChanneldReplicationComponent::SetStateToChannelData(const google::protobuf::Message* State, google::protobuf::Message* ChannelData, UClass* TargetClass, uint32 NetGUID)
{

}

void UChanneldReplicationComponent::PostInitProperties()
{
	Super::PostInitProperties();

//#if !WITH_EDITOR
	//InitOnce();
//#endif
}

void UChanneldReplicationComponent::InitOnce()
{
	if (!GetOwner())
		return;

	if (bInitialized)
		return;

	UGameInstance* GameInstance = GetOwner()->GetGameInstance();
	if (!GameInstance)
		return;

	auto Settings = GetMutableDefault<UChanneldSettings>();
	if (Settings->bSkipCustomReplication && Settings->bSkipCustomRPC)
	{
		return;
	}

	// TODO: OwnerOnly replication should use private channels
	if (GetOwner()->GetIsReplicated() || GetOwner()->bOnlyRelevantToOwner)
	{
		auto OwnerReplicators = ChanneldReplication::FindAndCreateReplicators(GetOwner());
		if (OwnerReplicators.Num() > 0)
		{
			Replicators.Append(OwnerReplicators);
		}
		else
		{
			UE_LOG(LogChanneld, Warning, TEXT("Unable to add replicator for owner actor: %s"), *GetOwner()->GetFullName());
		}
	}
	for (auto RepComp : GetOwner()->GetReplicatedComponents())
	{
		auto CompReplicators = ChanneldReplication::FindAndCreateReplicators(RepComp);
		if (CompReplicators.Num() > 0)
		{
			Replicators.Append(CompReplicators);
		}
		else
		{
			UE_LOG(LogChanneld, Warning, TEXT("Unable to add replicator for component '%s' of actor: %s"), *RepComp->GetName(), *GetOwner()->GetName());
		}
	}

	// Make sure the DataProvider is always registered
	GameInstance->GetSubsystem<UChanneldGameInstanceSubsystem>()->RegisterDataProvider(this);

	bInitialized = true;
}

void UChanneldReplicationComponent::BeginPlay()
{
	Super::BeginPlay();

	InitOnce();
}

void UChanneldReplicationComponent::UninitOnce()
{
	if (bUninitialized)
		return;

	UChannelDataView* View = GetOwner()->GetGameInstance()->GetSubsystem<UChanneldGameInstanceSubsystem>()->GetChannelDataView();
	if (View)
	{
		View->RemoveProviderFromAllChannels(this, GetNetMode() == ENetMode::NM_DedicatedServer);
	}

	bUninitialized = true;
}

void UChanneldReplicationComponent::EndPlay(EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);

	//if (Reason != EEndPlayReason::LevelTransition)
	{
		UninitOnce();
	}
}

void UChanneldReplicationComponent::DestroyComponent(bool bPromoteChildren /* = false */)
{
	Super::DestroyComponent(bPromoteChildren);

	//UninitOnce();
}

channeldpb::ChannelType UChanneldReplicationComponent::GetChannelType()
{
	return static_cast<channeldpb::ChannelType>(ChannelType);
}

google::protobuf::Message* UChanneldReplicationComponent::GetChannelDataTemplate() const
{
	auto MsgObj = ProvideChannelDataTemplate();
	if (MsgObj)
	{
		return MsgObj->GetMessage();
	}
	else
	{
		UE_LOG(LogChanneld, Error, TEXT("ChanneldActor '%s' failed to provider channel data template"), *GetFName().ToString());
		return nullptr;
	}
}

ChannelId UChanneldReplicationComponent::GetChannelId()
{
	return OwningChannelId;
}

void UChanneldReplicationComponent::SetChannelId(ChannelId ChId)
{
	OwningChannelId = ChId;
}

bool UChanneldReplicationComponent::IsRemoved()
{
	return bRemoved;
}

void UChanneldReplicationComponent::SetRemoved()
{
	bRemoved = true;
}

bool UChanneldReplicationComponent::UpdateChannelData(google::protobuf::Message* ChannelData)
{
	if (!GetOwner()->HasAuthority())
	{
		return false;
	}

	bool bUpdated = false;
	for (auto Replicator : Replicators)
	{
		Replicator->Tick(FApp::GetDeltaTime());
		if (Replicator->IsStateChanged())
		{
			uint32 NetGUID = Replicator->GetNetGUID();
			if (NetGUID == 0)
			{
				UE_LOG(LogChanneld, Log, TEXT("Replicator of %s doesn't has a NetGUID yet, skip setting channel data"), *Replicator->GetTargetClass()->GetName());
				continue;
			}
			SetStateToChannelData(Replicator->GetDeltaState(), ChannelData, Replicator->GetTargetClass(), NetGUID);
			Replicator->ClearState();
			bUpdated = true;
		}
	}

	return bUpdated;
}

void UChanneldReplicationComponent::OnChannelDataUpdated(google::protobuf::Message* ChannelData)
{
	if (GetOwner()->HasAuthority())
	{
		return;
	}

	for (auto Replicator : Replicators)
	{
		auto TargetObj = Replicator->GetTargetObject();
		if (!TargetObj)
		{
			UE_LOG(LogChanneld, Warning, TEXT("Replicator's target object is missing (maybe already destroyed)"));
			continue;
		}
		uint32 NetGUID = Replicator->GetNetGUID();
		if (NetGUID == 0)
		{
			// TODO: delay the state update
			UE_LOG(LogChanneld, Warning, TEXT("Replicator of %s.%s doesn't have a valid NetGUID (maybe the NetDriver is not initialized yet) to find the state."), 
				*Replicator->GetTargetObject()->GetName(), *Replicator->GetTargetClass()->GetName());
			continue;
		}
		bool bIsRemoved = false;
		auto State = GetStateFromChannelData(ChannelData, Replicator->GetTargetClass(), NetGUID, bIsRemoved);
		if (State)
		{
			if (bIsRemoved)
			{
				if (TargetObj == GetOwner())
				{
					GetOwner()->Destroy(true);
				}
				else if (TargetObj->IsA<UActorComponent>())
				{
					GetOwner()->RemoveOwnedComponent(Cast<UActorComponent>(TargetObj));
				}
				continue;
			}
			Replicator->OnStateChanged(State);
		}
	}
}

TSharedPtr<google::protobuf::Message> UChanneldReplicationComponent::SerializeFunctionParams(AActor* Actor, UFunction* Func, void* Params, bool& bSuccess)
{
	for (auto Replicator : Replicators)
	{
		if (Replicator->GetTargetObject() == Actor)
		{
			auto ParamsMsg = Replicator->SerializeFunctionParams(Func, Params, bSuccess);
			if (bSuccess)
			{
				return ParamsMsg;
			}
		}
	}

	bSuccess = false;
	return nullptr;
}

TSharedPtr<void> UChanneldReplicationComponent::DeserializeFunctionParams(AActor* Actor, UFunction* Func, const std::string& ParamsPayload, bool& bSuccess, bool& bDelayRPC)
{
	for (auto Replicator : Replicators)
	{
		if (Replicator->GetTargetObject() == Actor)
		{
			TSharedPtr<void> Params = Replicator->DeserializeFunctionParams(Func, ParamsPayload, bSuccess, bDelayRPC);
			if (bSuccess)
			{
				return Params;
			}
		}
	}

	bSuccess = false;
	return nullptr;
}
