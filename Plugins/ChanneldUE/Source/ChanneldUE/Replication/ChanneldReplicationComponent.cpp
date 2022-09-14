#include "ChanneldReplicationComponent.h"
#include "ChanneldGameInstanceSubsystem.h"
#include "ChannelDataProvider.h"
#include "View/ChannelDataView.h"
#include "Engine/ActorChannel.h"
#include "Engine/PackageMapClient.h"
#include "Misc/NetworkGuid.h"
#include "ChanneldReplication.h"

UChanneldReplicationComponent::UChanneldReplicationComponent(const FObjectInitializer& ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}

const google::protobuf::Message* UChanneldReplicationComponent::GetStateFromChannelData(google::protobuf::Message* ChannelData, UObject* TargetObject, uint32 NetGUID, bool& bIsRemoved)
{
	bIsRemoved = false;
	return nullptr;
}

void UChanneldReplicationComponent::SetStateToChannelData(const google::protobuf::Message* State, google::protobuf::Message* ChannelData, UObject* TargetObject, uint32 NetGUID)
{

}

// Called when the game starts or when spawned
void UChanneldReplicationComponent::BeginPlay()
{
	Super::BeginPlay();

	// TODO: OwnerOnly replication uses private channels
	if (GetOwner()->GetIsReplicated() || GetOwner()->bOnlyRelevantToOwner)
	{
		auto Replicator = ChanneldReplication::FindAndCreateReplicator(GetOwner());
		if (Replicator)
		{
			Replicators.Add(Replicator);
		}
		else
		{
			UE_LOG(LogChanneld, Warning, TEXT("Unable to add replicator for owner actor: %s"), *GetOwner()->GetFullName());
		}
	}
	for (auto RepComp : GetOwner()->GetReplicatedComponents())
	{
		auto Replicator = ChanneldReplication::FindAndCreateReplicator(RepComp);
		if (Replicator)
		{
			Replicators.Add(Replicator);
		}
		else
		{
			UE_LOG(LogChanneld, Warning, TEXT("Unable to add replicator for component '%s' of actor: %s"), *RepComp->GetFullName(), *GetOwner()->GetFullName());
		}
	}

	UChannelDataView* View = GetOwner()->GetGameInstance()->GetSubsystem<UChanneldGameInstanceSubsystem>()->GetChannelDataView();
	if (View)
	{
		View->AddProvider(OwningChannelId, this);
	}
}

void UChanneldReplicationComponent::EndPlay(EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);

	//if (Reason != EEndPlayReason::LevelTransition)
	{
		UChannelDataView* View = GetOwner()->GetGameInstance()->GetSubsystem<UChanneldGameInstanceSubsystem>()->GetChannelDataView();
		if (View)
		{
			View->RemoveProviderFromAllChannels(this, true);
		}
	}
}

// Called every frame
void UChanneldReplicationComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// TODO: update the replicators with the dynamically added/removed components

	/*
	for (auto Pair : SceneComponentReplicators)
	{
		Pair.Value->Tick(DeltaTime);
	}
	*/
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
			SetStateToChannelData(Replicator->GetState(), ChannelData, Replicator->GetTargetObject(), Replicator->GetNetGUID());
			Replicator->ClearState();
			bUpdated = true;
		}
	}

	return bUpdated;
}

void UChanneldReplicationComponent::OnChannelDataUpdated(google::protobuf::Message* ChannelData)
{
	// FIXME: don't update if the source (the connection that updated the channel data) is self.

	for (auto Replicator : Replicators)
	{
		auto TargetObj = Replicator->GetTargetObject();
		if (!TargetObj)
		{
			continue;
		}
		bool bIsRemoved = false;
		auto State = GetStateFromChannelData(ChannelData, Replicator->GetTargetObject(), Replicator->GetNetGUID(), bIsRemoved);
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

TSharedPtr<google::protobuf::Message> UChanneldReplicationComponent::SerializeFunctionParams(AActor* Actor, UFunction* Func, void* Params)
{
	for (auto Replicator : Replicators)
	{
		if (Replicator->GetTargetObject() == Actor)
		{
			return Replicator->SerializeFunctionParams(Func, Params);
		}
	}
	return nullptr;
}

void* UChanneldReplicationComponent::DeserializeFunctionParams(AActor* Actor, UFunction* Func, const std::string& ParamsPayload)
{
	for (auto Replicator : Replicators)
	{
		if (Replicator->GetTargetObject() == Actor)
		{
			return Replicator->DeserializeFunctionParams(Func, ParamsPayload);
		}
	}
	return nullptr;
}
