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

	FullState = new unrealpb::SceneComponentState;
	DeltaState = new unrealpb::SceneComponentState;

	// Prepare Reflection pointers
	{
		auto Property = CastFieldChecked<const FBoolProperty>(SceneComp->GetClass()->FindPropertyByName(FName("bShouldBeAttached")));
		bShouldBeAttachedPtr = Property->ContainerPtrToValuePtr<uint8>(SceneComp.Get());
		check(bShouldBeAttachedPtr);
	}
	{
		auto Property = CastFieldChecked<const FBoolProperty>(SceneComp->GetClass()->FindPropertyByName(FName("bShouldSnapLocationWhenAttached")));
		bShouldSnapLocationWhenAttachedPtr = Property->ContainerPtrToValuePtr<uint8>(SceneComp.Get());
		check(bShouldSnapLocationWhenAttachedPtr);
	}
	{
		auto Property = CastFieldChecked<const FBoolProperty>(SceneComp->GetClass()->FindPropertyByName(FName("bShouldSnapRotationWhenAttached")));
		bShouldSnapRotationWhenAttachedPtr = Property->ContainerPtrToValuePtr<uint8>(SceneComp.Get());
		check(bShouldSnapRotationWhenAttachedPtr);
	}
}

FChanneldSceneComponentReplicator::~FChanneldSceneComponentReplicator()
{
	if (SceneComp.IsValid())
	{
		SceneComp->TransformUpdated.RemoveAll(this);
	}

	delete FullState;
	delete DeltaState;
}

void FChanneldSceneComponentReplicator::Tick(float DeltaTime)
{
	if (!SceneComp.IsValid() || !SceneComp->GetOwner())
	{
		return;
	}

	// Only server can update channel data
	if (!SceneComp->GetOwner()->HasAuthority())
	{
		return;
	}

	if (FullState->babsolutelocation() != SceneComp->IsUsingAbsoluteLocation())
	{
		DeltaState->set_babsolutelocation(SceneComp->IsUsingAbsoluteLocation());
		bStateChanged = true;
	}

	if (FullState->babsoluterotation() != SceneComp->IsUsingAbsoluteRotation())
	{
		DeltaState->set_babsoluterotation(SceneComp->IsUsingAbsoluteRotation());
		bStateChanged = true;
	}

	if (FullState->babsolutescale() != SceneComp->IsUsingAbsoluteScale())
	{
		DeltaState->set_babsolutescale(SceneComp->IsUsingAbsoluteScale());
		bStateChanged = true;
	}

	if (FullState->bvisible() != SceneComp->IsVisible())
	{
		DeltaState->set_bvisible(SceneComp->IsVisible());
		bStateChanged = true;
	}

	if (FullState->bshouldbeattached() != (bool)*bShouldBeAttachedPtr)
	{
		DeltaState->set_bshouldbeattached(*bShouldBeAttachedPtr);
		bStateChanged = true;
	}

	if (FullState->bshouldsnaplocationwhenattached() != (bool)*bShouldSnapLocationWhenAttachedPtr)
	{
		DeltaState->set_bshouldsnaplocationwhenattached(*bShouldSnapLocationWhenAttachedPtr);
		bStateChanged = true;
	}

	if (FullState->bshouldsnaprotationwhenattached() != (bool)*bShouldSnapRotationWhenAttachedPtr)
	{
		DeltaState->set_bshouldsnaprotationwhenattached(*bShouldSnapRotationWhenAttachedPtr);
		bStateChanged = true;
	}

	UObject* AttachParent = ChanneldUtils::GetObjectByRef(FullState->mutable_attachparent(), SceneComp->GetWorld());
	if (AttachParent != SceneComp->GetAttachParent())
	{
		if (SceneComp->GetAttachParent() == nullptr)
		{
			// Catch warn_unused_result error for Linux build
			auto result = DeltaState->release_attachparent();
			result = nullptr;
		}
		else
		{
			DeltaState->mutable_attachparent()->MergeFrom(ChanneldUtils::GetRefOfObject(SceneComp->GetAttachParent()));
		}
		bStateChanged = true;
	}

	if (FullState->mutable_attachchildren()->size() != SceneComp->GetAttachChildren().Num())
	{
		DeltaState->clear_attachchildren();
		for (auto Child : SceneComp->GetAttachChildren())
		{
			*DeltaState->mutable_attachchildren()->Add() = ChanneldUtils::GetRefOfObject(Child);
		}
		bStateChanged = true;
	}

	FName AttachSocketName(FullState->mutable_attachsocketname()->c_str());
	if (AttachSocketName != SceneComp->GetAttachSocketName())
	{
		*DeltaState->mutable_attachsocketname() = TCHAR_TO_UTF8(*SceneComp->GetAttachSocketName().ToString());
		bStateChanged = true;
	}

	if (ChanneldUtils::SetIfNotSame(FullState->mutable_relativelocation(), SceneComp->GetRelativeLocation()))
	{
		bStateChanged = true;
		DeltaState->mutable_relativelocation()->MergeFrom(*FullState->mutable_relativelocation());
	}

	if (ChanneldUtils::SetIfNotSame(FullState->mutable_relativerotation(), SceneComp->GetRelativeRotation().Vector()))
	{
		bStateChanged = true;
		DeltaState->mutable_relativerotation()->MergeFrom(*FullState->mutable_relativerotation());
	}

	if (ChanneldUtils::SetIfNotSame(FullState->mutable_relativescale(), SceneComp->GetRelativeScale3D()))
	{
		bStateChanged = true;
		DeltaState->mutable_relativescale()->MergeFrom(*FullState->mutable_relativescale());
	}

	// TODO: Optimization: Set the FullState as well as the DeltaState above
	FullState->MergeFrom(*DeltaState);
}

void FChanneldSceneComponentReplicator::ClearState()
{
	DeltaState->Clear();
	bStateChanged = false;
}

void FChanneldSceneComponentReplicator::OnStateChanged(const google::protobuf::Message* InNewState)
{
	if (!SceneComp.IsValid() || !SceneComp->GetOwner())
	{
		return;
	}

	// Only proxy need to update from the channel data
	if (SceneComp->GetOwner()->HasAuthority())
	{
		return;
	}

	auto NewState = static_cast<const unrealpb::SceneComponentState*>(InNewState);
	FullState->CopyFrom(*NewState);
	bStateChanged = false;

	bool bTransformChanged = false;
	FVector NewLocation = SceneComp->GetRelativeLocation();
	FRotator NewRotation = SceneComp->GetRelativeRotation();
	if (NewState->has_relativelocation())
	{
		NewLocation = ChanneldUtils::GetVector(NewState->relativelocation());
		SceneComp->SetRelativeLocation_Direct(NewLocation);
		bTransformChanged = true;
	}

	if (NewState->has_relativerotation())
	{
		NewRotation = ChanneldUtils::GetRotator(NewState->relativerotation());
		SceneComp->SetRelativeRotation_Direct(NewRotation);
		bTransformChanged = true;
	}

	if (NewState->has_relativescale())
	{
		SceneComp->SetRelativeScale3D_Direct(ChanneldUtils::GetVector(NewState->relativescale()));
		bTransformChanged = true;
	}

	if (NewState->has_babsolutelocation() && NewState->babsolutelocation() != SceneComp->IsUsingAbsoluteLocation())
	{
		SceneComp->SetUsingAbsoluteLocation(NewState->babsolutelocation());
		bTransformChanged = true;
	}

	if (NewState->has_babsoluterotation() && NewState->babsoluterotation() != SceneComp->IsUsingAbsoluteRotation())
	{
		SceneComp->SetUsingAbsoluteRotation(NewState->babsoluterotation());
		bTransformChanged = true;
	}

	if (NewState->has_babsolutescale() && NewState->babsolutescale() != SceneComp->IsUsingAbsoluteScale())
	{
		SceneComp->SetUsingAbsoluteScale(NewState->babsolutescale());
		bTransformChanged = true;
	}

	if (bTransformChanged)
	{
		SceneComp->UpdateComponentToWorld();
		//SceneComp->SetRelativeLocationAndRotation(NewLocation, NewRotation);
	}

	if (NewState->has_attachparent())
	{
		auto AttachParent = Cast<USceneComponent>(ChanneldUtils::GetObjectByRef(&NewState->attachparent(), SceneComp->GetWorld()));
		if (AttachParent && AttachParent != SceneComp->GetAttachParent())
		{
			FName AttachSocketName; 
			if (NewState->has_attachsocketname())
			{
				AttachSocketName = NewState->attachsocketname().c_str();
			}
			else
			{
				AttachSocketName = SceneComp->GetAttachSocketName();
			}

			SceneComp->AttachToComponent(AttachParent, FAttachmentTransformRules(
				GetAttachmentRule(FullState->bshouldsnaplocationwhenattached(), SceneComp->IsUsingAbsoluteLocation()),
				GetAttachmentRule(FullState->bshouldsnaprotationwhenattached(), SceneComp->IsUsingAbsoluteRotation()),
				GetAttachmentRule(false, SceneComp->IsUsingAbsoluteScale()),
				false));
		}
	}

	if (NewState->has_bvisible() && NewState->bvisible() != SceneComp->IsVisible())
	{
		SceneComp->SetVisibility(NewState->bvisible());
	}

	if (NewState->has_bshouldbeattached() && NewState->bshouldbeattached() != (bool)*bShouldBeAttachedPtr)
	{
		*bShouldBeAttachedPtr = NewState->bshouldbeattached();
	}
	if (NewState->has_bshouldsnaplocationwhenattached() && NewState->bshouldsnaplocationwhenattached() != (bool)*bShouldSnapLocationWhenAttachedPtr)
	{
		*bShouldSnapLocationWhenAttachedPtr = NewState->bshouldsnaplocationwhenattached();
	}
	if (NewState->has_bshouldsnaprotationwhenattached() && NewState->bshouldsnaprotationwhenattached() != (bool)*bShouldSnapRotationWhenAttachedPtr)
	{
		*bShouldSnapRotationWhenAttachedPtr = NewState->bshouldsnaprotationwhenattached();
	}

	// TODO: attachChildren
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