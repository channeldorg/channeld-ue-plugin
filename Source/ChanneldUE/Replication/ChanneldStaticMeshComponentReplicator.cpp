#include "ChanneldStaticMeshComponentReplicator.h"

FChanneldStaticMeshComponentReplicator::FChanneldStaticMeshComponentReplicator(UObject* InTargetObj) :
	FChanneldReplicatorBase(InTargetObj)
{
	Comp = CastChecked<UStaticMeshComponent>(InTargetObj);
	// Remove the registered DOREP() properties in the Actor
	TArray<FLifetimeProperty> RepProps;
	DisableAllReplicatedPropertiesOfClass(InTargetObj->GetClass(), GetTargetClass(), EFieldIteratorFlags::ExcludeSuper,RepProps);

	FullState = new unrealpb::StaticMeshComponentState;
	DeltaState = new unrealpb::StaticMeshComponentState;

	{
		auto Property = CastFieldChecked<const FObjectProperty>(InTargetObj->GetClass()->FindPropertyByName(FName("StaticMesh")));
		StaticMeshPtr = Property->ContainerPtrToValuePtr<UObject*>(InTargetObj);
		check(StaticMeshPtr);
	}

	OnRep_StaicMeshFunc = InTargetObj->GetClass()->FindFunctionByName(FName(TEXT("OnRep_StaticMesh")));
	check(OnRep_StaicMeshFunc);
}

FChanneldStaticMeshComponentReplicator::~FChanneldStaticMeshComponentReplicator()
{
	delete FullState;
	delete DeltaState;
}

google::protobuf::Message* FChanneldStaticMeshComponentReplicator::GetDeltaState()
{
	return DeltaState;
}

void FChanneldStaticMeshComponentReplicator::ClearState()
{
	DeltaState->Clear();
	bStateChanged = false;
}

uint32 FChanneldStaticMeshComponentReplicator::GetNetGUID()
{
	if (!NetGUID.IsValid())
	{
		if (Comp.IsValid())
		{
			UWorld* World = Comp->GetWorld();
			if (World && World->GetNetDriver())
			{
				NetGUID = World->GetNetDriver()->GuidCache->GetNetGUID(Comp->GetOwner());
			}
		}
	}
	return NetGUID.Value;
}

void FChanneldStaticMeshComponentReplicator::Tick(float DeltaTime)
{
	if (!Comp.IsValid() || !Comp->GetOwner())
	{
		return;
	}

	if (*StaticMeshPtr != ChanneldUtils::GetAssetByRef(&FullState->staticmesh()))
	{
		DeltaState->mutable_staticmesh()->CopyFrom(ChanneldUtils::GetAssetRef(*StaticMeshPtr));
		bStateChanged = true;
	}

	if (bStateChanged)
	{
		FullState->MergeFrom(*DeltaState);
	}
}

void FChanneldStaticMeshComponentReplicator::OnStateChanged(const google::protobuf::Message* InNewState)
{
	if (!Comp.IsValid() || !Comp->GetOwner())
	{
		return;
	}

	// Only client needs to apply the new state
	if (Comp->GetOwner()->HasAuthority())
	{
		return;
	}

	const unrealpb::StaticMeshComponentState* NewState = static_cast<const unrealpb::StaticMeshComponentState*>(InNewState);
	FullState->MergeFrom(*NewState);
	bStateChanged = false;

	if (NewState->has_staticmesh() && *StaticMeshPtr != ChanneldUtils::GetAssetByRef(&NewState->staticmesh()))
	{
		*StaticMeshPtr = ChanneldUtils::GetAssetByRef(&NewState->staticmesh());
		Comp->ProcessEvent(Comp->GetClass()->FindFunctionByName(FName(TEXT("OnRep_StaticMesh"))), &(*StaticMeshPtr));
	}
}
