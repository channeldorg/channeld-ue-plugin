#include "ChanneldSceneComponentReplicator.h"
#include "unreal_common.pb.h"
#include "ChanneldUtils.h"
#include "Net/UnrealNetwork.h"

FChanneldSceneComponentReplicator::FChanneldSceneComponentReplicator(USceneComponent* InSceneComp) : FChanneldReplicatorBase(InSceneComp)
{
	SceneComp = InSceneComp;

	// Remove the registered DOREP() properties in the SceneComponent
	TArray<FLifetimeProperty> RepProps;
	DisableAllReplicatedPropertiesOfClass(InSceneComp->GetClass(), USceneComponent::StaticClass(), EFieldIteratorFlags::ExcludeSuper, RepProps);

	SceneComp->TransformUpdated.AddRaw(this, &FChanneldSceneComponentReplicator::OnTransformUpdated);

	State = new unrealpb::SceneComponentState;
	RelativeLocationState = new unrealpb::FVector;
	RelativeRotationState = new unrealpb::FVector;
	RelativeScaleState = new unrealpb::FVector;
}

FChanneldSceneComponentReplicator::~FChanneldSceneComponentReplicator()
{
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
	if (!SceneComp.IsValid() || !SceneComp->GetOwner())
	{
		return;
	}

	if (State->babsolutelocation() != SceneComp->IsUsingAbsoluteLocation())
	{
		State->set_babsolutelocation(SceneComp->IsUsingAbsoluteLocation());
		bStateChanged = true;
	}

	if (State->babsoluterotation() != SceneComp->IsUsingAbsoluteRotation())
	{
		State->set_babsoluterotation(SceneComp->IsUsingAbsoluteRotation());
		bStateChanged = true;
	}

	if (State->babsolutescale() != SceneComp->IsUsingAbsoluteScale())
	{
		State->set_babsolutescale(SceneComp->IsUsingAbsoluteScale());
		bStateChanged = true;
	}

	if (State->bvisible() != SceneComp->IsVisible())
	{
		State->set_bvisible(SceneComp->IsVisible());
		bStateChanged = true;
	}

	/* TODO: members are inaccessible, try use reflection? 
	if (State->bshouldbeattached() != SceneComp->bShouldSnapLocationWhenAttached)
	{
		State->set_bshouldbeattached(SceneComp->bShouldSnapLocationWhenAttached);
		bStateChanged = true;
	}

	if (State->bshouldsnaplocationwhenattached() != SceneComp->bShouldSnapLocationWhenAttached)
	{
		State->set_bshouldsnaplocationwhenattached(SceneComp->bShouldSnapLocationWhenAttached);
		bStateChanged = true;
	}

	if (State->bshouldsnaprotationwhenattached() != SceneComp->bShouldSnapRotationWhenAttached)
	{
		State->set_bshouldsnaprotationwhenattached(SceneComp->bShouldSnapRotationWhenAttached);
		bStateChanged = true;
	}
	*/

	UObject* AttachParent = ChanneldUtils::GetObjectByRef(State->mutable_attachparent(), SceneComp->GetWorld());
	if (AttachParent != SceneComp->GetAttachParent())
	{
		if (SceneComp->GetAttachParent() == nullptr)
		{
			State->release_attachparent();
		}
		else
		{
			State->mutable_attachparent()->MergeFrom(ChanneldUtils::GetRefOfObject(SceneComp->GetAttachParent()));
		}
		bStateChanged = true;
	}

	if (State->mutable_attachchildren()->size() != SceneComp->GetAttachChildren().Num())
	{
		State->clear_attachchildren();
		for (auto Child : SceneComp->GetAttachChildren())
		{
			*State->mutable_attachchildren()->Add() = ChanneldUtils::GetRefOfObject(Child);
		}
		bStateChanged = true;
	}

	FName AttachSocketName(State->mutable_attachsocketname()->c_str());
	if (AttachSocketName != SceneComp->GetAttachSocketName())
	{
		*State->mutable_attachsocketname() = TCHAR_TO_UTF8(*SceneComp->GetAttachSocketName().ToString());
		bStateChanged = true;
	}

	/* Moved to ClearState()
	if (bStateChanged)
	{
		Actor->UpdateSceneComponent(State);
		State->Clear();
		bStateChanged = false;
	}
	*/
}

void FChanneldSceneComponentReplicator::ClearState()
{
	FChanneldReplicatorBase::ClearState();
	State->release_relativelocation();
	State->release_relativerotation();
	State->release_relativescale();
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
	if (!SceneComp.IsValid() || !SceneComp->GetOwner())
	{
		return;
	}

	if (!SceneComp->GetOwner()->HasAuthority())
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

void FChanneldSceneComponentReplicator::OnStateChanged(const google::protobuf::Message* NewState)
{
	if (!SceneComp.IsValid() || !SceneComp->GetOwner())
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

	if (State->babsolutelocation() != SceneComp->IsUsingAbsoluteLocation())
	{
		SceneComp->SetUsingAbsoluteLocation(State->babsolutelocation());
		bTransformChanged = true;
	}

	if (State->babsoluterotation() != SceneComp->IsUsingAbsoluteRotation())
	{
		SceneComp->SetUsingAbsoluteRotation(State->babsoluterotation());
		bTransformChanged = true;
	}

	if (State->babsolutescale() != SceneComp->IsUsingAbsoluteScale())
	{
		SceneComp->SetUsingAbsoluteScale(State->babsolutescale());
		bTransformChanged = true;
	}

	if (bTransformChanged)
	{
		SceneComp->UpdateComponentToWorld();
		//SceneComp->SetRelativeLocationAndRotation(NewLocation, NewRotation);
	}

	if (State->has_attachparent())
	{
		auto AttachParent = Cast<USceneComponent>(ChanneldUtils::GetObjectByRef(&State->attachparent(), SceneComp->GetWorld()));
		if (AttachParent && AttachParent != SceneComp->GetAttachParent())
		{
			FName AttachSocketName; 
			if (State->attachsocketname().length() > 0)
			{
				AttachSocketName = State->mutable_attachsocketname()->c_str();
			}
			else
			{
				AttachSocketName = SceneComp->GetAttachSocketName();
			}

			SceneComp->AttachToComponent(AttachParent, FAttachmentTransformRules(
				GetAttachmentRule(State->bshouldsnaplocationwhenattached(), SceneComp->IsUsingAbsoluteLocation()),
				GetAttachmentRule(State->bshouldsnaprotationwhenattached(), SceneComp->IsUsingAbsoluteRotation()),
				GetAttachmentRule(false, SceneComp->IsUsingAbsoluteScale()),
				false));
		}
	}

	if (State->bvisible() != SceneComp->IsVisible())
	{
		SceneComp->SetVisibility(State->bvisible());
	}

	// TODO: bShouldBeAttached, attachChildren
}

EAttachmentRule FChanneldSceneComponentReplicator::GetAttachmentRule(bool bShouldSnapWhenAttached, bool bAbsolute)
{
	if (bShouldSnapWhenAttached)
	{
		return EAttachmentRule::SnapToTarget;
	}
	else if (bAbsolute)
	{
		return EAttachmentRule::KeepWorld;
	}
	else
	{
		return EAttachmentRule::KeepRelative;
	}
}