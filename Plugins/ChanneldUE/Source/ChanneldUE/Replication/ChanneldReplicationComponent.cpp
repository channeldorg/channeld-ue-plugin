#include "ChanneldReplicationComponent.h"
#include "ChanneldGameInstanceSubsystem.h"
#include "ChannelDataProvider.h"
#include "View/ChannelDataView.h"
#include "Engine/ActorChannel.h"
#include "Engine/PackageMapClient.h"
#include "Misc/NetworkGuid.h"

UChanneldReplicationComponent::UChanneldReplicationComponent(const FObjectInitializer& ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void UChanneldReplicationComponent::BeginPlay()
{
	Super::BeginPlay();

	for (auto RepComp : GetOwner()->GetReplicatedComponents())
	{
		if (RepComp->IsA<USceneComponent>())
		{
			USceneComponent* SceneComp = Cast<USceneComponent>(RepComp);
			SceneComponentReplicators.Add(new FChanneldSceneComponentReplicator(SceneComp, GetOwner()));
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
	for (auto Replicator : SceneComponentReplicators)
	{
		Replicator->Tick(0);
		if (Replicator->IsStateChanged())
		{
			SetSceneComponentStateToChannelData(Replicator->GetState(), ChannelData, Replicator->GetNetGUID());
			Replicator->ClearState();
			bUpdated = true;
		}
	}

	return bUpdated;
}

void UChanneldReplicationComponent::OnChannelDataUpdated(google::protobuf::Message* ChannelData)
{
	// FIXME: don't update if the source (the connection that updated the channel data) is self.

	for (auto Replicator : SceneComponentReplicators)
	{
		auto State = GetSceneComponentStateFromChannelData(ChannelData, Replicator->GetNetGUID());
		if (State)
		{
			if (State->removed())
			{
				auto SceneComp = Replicator->GetSceneComponent();
				if (SceneComp)
				{
					GetOwner()->RemoveOwnedComponent(SceneComp);
				}
				continue;
			}
			Replicator->OnStateChanged(State);
		}
	}
}
