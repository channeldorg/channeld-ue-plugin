#include "ChanneldActorComponentReplicator.h"
#include "Net/UnrealNetwork.h"
#include "ChanneldUtils.h"

FChanneldActorComponentReplicator::FChanneldActorComponentReplicator(UObject* InTargetObj) : FChanneldReplicatorBase(InTargetObj)
{
	ActorComponent = CastChecked<UActorComponent>(InTargetObj);
	// Remove the registered DOREP() properties in the Character
	TArray<FLifetimeProperty> RepProps;
	DisableAllReplicatedPropertiesOfClass(InTargetObj->GetClass(), UActorComponent::StaticClass(), EFieldIteratorFlags::ExcludeSuper, RepProps);

	FullState = new unrealpb::ActorComponentState;
	DeltaState = new unrealpb::ActorComponentState;
}

uint32 FChanneldActorComponentReplicator::GetNetGUID()
{
	if (!NetGUID.IsValid())
	{
		if (ActorComponent.IsValid())
		{
			UWorld* World = ActorComponent->GetWorld();
			if (World && World->GetNetDriver())
			{
				NetGUID = World->GetNetDriver()->GuidCache->GetNetGUID(ActorComponent->GetOwner());
			}
		}
	}
	return NetGUID.Value;
}

FChanneldActorComponentReplicator::~FChanneldActorComponentReplicator()
{
	delete FullState;
	delete DeltaState;
}

google::protobuf::Message* FChanneldActorComponentReplicator::GetDeltaState()
{
	return DeltaState;
}

void FChanneldActorComponentReplicator::ClearState()
{
	DeltaState->Clear();
	bStateChanged = false;
}

void FChanneldActorComponentReplicator::Tick(float DeltaTime)
{
	if (!ActorComponent.IsValid())
	{
		return;
	}

	// Only server can update channel data
	if (!ActorComponent->GetOwner()->HasAuthority())
	{
		return;
	}

	if (ActorComponent->IsActive() != FullState->bisactive())
	{
		DeltaState->set_bisactive(ActorComponent->IsActive());
		bStateChanged = true;
	}
	if (ActorComponent->GetIsReplicated() != FullState->breplicated())
	{
		DeltaState->set_breplicated(ActorComponent->GetIsReplicated());
		bStateChanged = true;
	}

	FullState->MergeFrom(*DeltaState);
}

void FChanneldActorComponentReplicator::OnStateChanged(const google::protobuf::Message* InNewState)
{
	if (!ActorComponent.IsValid())
	{
		return;
	}

	auto NewState = static_cast<const unrealpb::ActorComponentState*>(InNewState);
	FullState->MergeFrom(*NewState);
	bStateChanged = false;

	if (NewState->has_bisactive())
	{
		ActorComponent->SetActiveFlag(NewState->bisactive());
		ActorComponent->OnRep_IsActive();
	}

	if (NewState->has_breplicated())
	{
		ActorComponent->SetIsReplicated(NewState->breplicated());
	}
}

