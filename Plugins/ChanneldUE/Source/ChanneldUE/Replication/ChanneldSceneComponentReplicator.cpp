#include "ChanneldSceneComponentReplicator.h"
#include "unreal_common.pb.h"
#include "ChanneldActor.h"
#include "ChanneldUtils.h"
#include "Net/UnrealNetwork.h"

FChanneldSceneComponentReplicator::FChanneldSceneComponentReplicator(USceneComponent* InSceneComp, AChanneldActor* InActor)
{
	SceneComp = InSceneComp;
	Actor = InActor;

	// Remove the registered DOREP() properties in the SceneComponent
	TArray<FLifetimeProperty> RepProps;
	DisableAllReplicatedPropertiesOfClass(InSceneComp->GetClass(), USceneComponent::StaticClass(), EFieldIteratorFlags::ExcludeSuper, RepProps);

	SceneComp->TransformUpdated.AddRaw(this, &FChanneldSceneComponentReplicator::OnTransformUpdated);
	Actor->OnSceneComponentUpdated.AddRaw(this, &FChanneldSceneComponentReplicator::OnStateChanged);

	bStateChanged = false;
	State = new unrealpb::SceneComponentState;
	RelativeLocationState = new unrealpb::FVector;
	RelativeRotationState = new unrealpb::FVector;
	RelativeScaleState = new unrealpb::FVector;
}

FChanneldSceneComponentReplicator::~FChanneldSceneComponentReplicator()
{
	Actor->OnSceneComponentUpdated.RemoveAll(this);
	
	if (SceneComp.IsValid())
	{
		SceneComp->TransformUpdated.RemoveAll(this);
	}

	delete State;
	delete RelativeLocationState;
	delete RelativeRotationState;
	delete RelativeScaleState;
}

void FChanneldSceneComponentReplicator::Tick(float DeltaTime)
{
	if (!SceneComp.IsValid())
	{
		return;
	}

	if (State->isvisible() != SceneComp->IsVisible())
	{
		State->set_isvisible(SceneComp->IsVisible());
		bStateChanged = true;
	}

	// TODO: AttachSocketName and other properties...

	/*
	if (bStateChanged)
	{
		Actor->UpdateSceneComponent(State);
		State->Clear();
		bStateChanged = false;
	}
	*/
}

bool FChanneldSceneComponentReplicator::SetIfNotSame(unrealpb::FVector* VectorToSet, const FVector& VectorToCheck)
{
	bool bNotSame = false;
	if (!FMath::IsNearlyEqual(VectorToSet->x(), VectorToCheck.X))
	{
		VectorToSet->set_x(VectorToCheck.X);
		bNotSame = true;
	}
	if (!FMath::IsNearlyEqual(VectorToSet->y(), VectorToCheck.Y))
	{
		VectorToSet->set_y(VectorToCheck.Y);
		bNotSame = true;
	}
	if (!FMath::IsNearlyEqual(VectorToSet->z(), VectorToCheck.Z))
	{
		VectorToSet->set_z(VectorToCheck.Z);
		bNotSame = true;
	}
	return bNotSame;
}

void FChanneldSceneComponentReplicator::OnTransformUpdated(USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	if (!Actor->HasAuthority())
	{
		return;
	}

	if (!SceneComp.IsValid())
	{
		return;
	}

	if (SetIfNotSame(RelativeLocationState, SceneComp->GetRelativeLocation()))
	{
		bStateChanged = true;
		State->mutable_relativelocation()->MergeFrom(*RelativeLocationState);
	}

	if (SetIfNotSame(RelativeRotationState, SceneComp->GetRelativeRotation().Vector()))
	{
		bStateChanged = true;
		State->mutable_relativerotation()->MergeFrom(*RelativeRotationState);
	}

	if (SetIfNotSame(RelativeScaleState, SceneComp->GetRelativeScale3D()))
	{
		bStateChanged = true;
		State->mutable_relativescale()->MergeFrom(*RelativeScaleState);
	}
}

void FChanneldSceneComponentReplicator::OnStateChanged(const unrealpb::SceneComponentState* NewState)
{
	if (!SceneComp.IsValid())
	{
		return;
	}

	State->CopyFrom(*NewState);
	bStateChanged = false;

	bool bTransformChanged = false;
	FVector NewLocation = SceneComp->GetRelativeLocation();
	FRotator NewRotation = SceneComp->GetRelativeRotation();
	if (State->has_relativelocation())
	{
		NewLocation = ChanneldUtils::GetVector(State->relativelocation());
		SceneComp->SetRelativeLocation_Direct(NewLocation);
		bTransformChanged = true;
	}

	if (State->has_relativerotation())
	{
		NewRotation = ChanneldUtils::GetRotator(State->relativerotation());
		SceneComp->SetRelativeRotation_Direct(NewRotation);
		bTransformChanged = true;
	}

	if (State->has_relativescale())
	{
		SceneComp->SetRelativeScale3D_Direct(ChanneldUtils::GetVector(State->relativescale()));
		bTransformChanged = true;
	}

	if (bTransformChanged)
	{
		SceneComp->UpdateComponentToWorld();
		//SceneComp->SetRelativeLocationAndRotation(NewLocation, NewRotation);
	}
}
