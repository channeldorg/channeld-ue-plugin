#include "ChanneldActorComponentReplicator.h"
#include "Net/UnrealNetwork.h"
#include "ChanneldUtils.h"

FChanneldActorComponentReplicator::FChanneldActorComponentReplicator(UObject* InTargetObj) : FChanneldReplicatorBase_AC(InTargetObj)
{
	ActorComponent = CastChecked<UActorComponent>(InTargetObj);
	// Remove the registered DOREP() properties in the Character
	TArray<FLifetimeProperty> RepProps;
	DisableAllReplicatedPropertiesOfClass(InTargetObj->GetClass(), UActorComponent::StaticClass(), EFieldIteratorFlags::ExcludeSuper, RepProps);

	TargetComponentName = TCHAR_TO_UTF8(*InTargetObj->GetName());
	FullState = new unrealpb::ActorComponentState;
	DeltaState = new unrealpb::ActorComponentState;
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

	if (bStateChanged)
	{
		FullState->MergeFrom(*DeltaState);
		FullState->set_compname(TargetComponentName);
	}
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
	}

	if (NewState->has_breplicated())
	{
		ActorComponent->SetIsReplicated(NewState->breplicated());
	}

	// Process the RepNotify functions after all the replicated properties are updated.
	if (NewState->has_bisactive())
	{
		ActorComponent->OnRep_IsActive();
	}
}

