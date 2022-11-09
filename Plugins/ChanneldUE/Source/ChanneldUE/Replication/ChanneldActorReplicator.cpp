#include "ChanneldActorReplicator.h"
#include "Net/UnrealNetwork.h"
#include "ChanneldUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"
#include "ChanneldConnection.h"
#include "GameFramework/PlayerState.h"

FChanneldActorReplicator::FChanneldActorReplicator(UObject* InTargetObj) : FChanneldReplicatorBase(InTargetObj)
{
	Actor = CastChecked<AActor>(InTargetObj);
	if (Actor->IsA<AGameStateBase>())
	{
		NetGUID = 1;
	}

	// Remove the registered DOREP() properties in the Character
	TArray<FLifetimeProperty> RepProps;
	DisableAllReplicatedPropertiesOfClass(InTargetObj->GetClass(), AActor::StaticClass(), EFieldIteratorFlags::ExcludeSuper, RepProps);

	FullState = new unrealpb::ActorState;
	DeltaState = new unrealpb::ActorState;

	{
		auto Property = CastFieldChecked<const FByteProperty>(Actor->GetClass()->FindPropertyByName(FName("RemoteRole")));
		RemoteRolePtr = Property->ContainerPtrToValuePtr<uint8>(Actor.Get());
		check(RemoteRolePtr);
	}
	{
		auto Property = CastFieldChecked<const FBoolProperty>(Actor->GetClass()->FindPropertyByName(FName("bTearOff")));
		bTearOffPtr = Property->ContainerPtrToValuePtr<bool>(Actor.Get());
		check(bTearOffPtr);
	}
	{
		auto Property = CastFieldChecked<const FStructProperty>(Actor->GetClass()->FindPropertyByName(FName("ReplicatedMovement")));
		ReplicatedMovementPtr = Property->ContainerPtrToValuePtr<FRepMovement>(Actor.Get());
		check(ReplicatedMovementPtr);
	}
	{
		auto Property = CastFieldChecked<const FStructProperty>(Actor->GetClass()->FindPropertyByName(FName("AttachmentReplication")));
		AttachmentReplicationPtr = Property->ContainerPtrToValuePtr<FRepAttachment>(Actor.Get());
		check(AttachmentReplicationPtr);
	}

	OnRep_OwnerFunc = Actor->GetClass()->FindFunctionByName(FName("OnRep_Owner"));
	check(OnRep_OwnerFunc);

	OnRep_ReplicatedMovementFunc = Actor->GetClass()->FindFunctionByName(FName("OnRep_ReplicatedMovement"));
	check(OnRep_ReplicatedMovementFunc);
}

FChanneldActorReplicator::~FChanneldActorReplicator()
{
	delete FullState;
	delete DeltaState;
}

google::protobuf::Message* FChanneldActorReplicator::GetDeltaState()
{
	return DeltaState;
}

void FChanneldActorReplicator::ClearState()
{
	DeltaState->Clear();
	bStateChanged = false;
}

void FChanneldActorReplicator::Tick(float DeltaTime)
{
	if (!Actor.IsValid())
	{
		return;
	}

	// Only server can update channel data
	if (!Actor->HasAuthority())
	{
		return;
	}

	auto Connnection = Cast<UChanneldNetConnection>(Actor->GetNetConnection());
	if (IsValid(Connnection) && Connnection->GetConnId() != FullState->owningconnid())
	{
		DeltaState->set_owningconnid(Connnection->GetConnId());
		bStateChanged = true;
	}
	if (Actor->IsReplicatingMovement() != FullState->breplicatemovement())
	{
		DeltaState->set_breplicatemovement(Actor->IsReplicatingMovement());
		bStateChanged = true;
	}
	if ((uint32)Actor->GetLocalRole() != FullState->localrole())
	{
		DeltaState->set_localrole(Actor->GetLocalRole());
		bStateChanged = true;
	}
	if ((uint32)Actor->GetRemoteRole() != FullState->remoterole())
	{
		DeltaState->set_remoterole(Actor->GetRemoteRole());
		bStateChanged = true;
	}

	AActor* Owner = Cast<AActor>(ChanneldUtils::GetObjectByRef(FullState->mutable_owner(), Actor->GetWorld(), false));
	if (Actor->GetOwner() != Owner)
	{
		DeltaState->mutable_owner()->CopyFrom(ChanneldUtils::GetRefOfObject(Actor->GetOwner()));
		bStateChanged = true;
	}

	if (Actor->IsHidden() != FullState->bhidden())
	{
		DeltaState->set_bhidden(Actor->IsHidden());
		bStateChanged = true;
	}
	if (Actor->GetTearOff() != FullState->btearoff())
	{
		DeltaState->set_btearoff(Actor->GetTearOff());
		bStateChanged = true;
	}

	AActor* Instigator = Cast<AActor>(ChanneldUtils::GetObjectByRef(FullState->mutable_instigator(), Actor->GetWorld(), false));
	if (Actor->GetInstigator() != Instigator)
	{
		DeltaState->mutable_instigator()->CopyFrom(ChanneldUtils::GetRefOfObject(Actor->GetInstigator()));
		bStateChanged = true;
	}

	//---------------------------------------------
	// AActor::PreReplication
	//---------------------------------------------
	Actor->GatherCurrentMovement();

	if (Actor->IsReplicatingMovement())
	{
		FRepMovement& RepMovement = *ReplicatedMovementPtr;
		unrealpb::FRepMovement* RepMovementFullState = FullState->mutable_replicatedmovement();
		unrealpb::FRepMovement* RepMovementDeltaState = DeltaState->mutable_replicatedmovement();
		if (ChanneldUtils::SetIfNotSame(RepMovementFullState->mutable_linearvelocity(), RepMovement.LinearVelocity))
		{
			ChanneldUtils::SetIfNotSame(RepMovementDeltaState->mutable_linearvelocity(), RepMovement.LinearVelocity);
			bStateChanged = true;
		}
		if (ChanneldUtils::SetIfNotSame(RepMovementFullState->mutable_angularvelocity(), RepMovement.AngularVelocity))
		{
			ChanneldUtils::SetIfNotSame(RepMovementDeltaState->mutable_angularvelocity(), RepMovement.AngularVelocity);
			bStateChanged = true;
		}
		if (ChanneldUtils::SetIfNotSame(RepMovementFullState->mutable_location(), RepMovement.Location))
		{
			ChanneldUtils::SetIfNotSame(RepMovementDeltaState->mutable_location(), RepMovement.Location);
			bStateChanged = true;
		}
		if (ChanneldUtils::SetIfNotSame(RepMovementFullState->mutable_rotation(), RepMovement.Rotation))
		{
			ChanneldUtils::SetIfNotSame(RepMovementDeltaState->mutable_rotation(), RepMovement.Rotation);
			bStateChanged = true;
		}
		if (RepMovement.bSimulatedPhysicSleep != RepMovementFullState->bsimulatedphysicsleep())
		{
			RepMovementDeltaState->set_bsimulatedphysicsleep(RepMovement.bSimulatedPhysicSleep);
			bStateChanged = true;
		}
		if (RepMovement.bRepPhysics != RepMovementFullState->brepphysics())
		{
			RepMovementDeltaState->set_brepphysics(RepMovement.bRepPhysics);
			bStateChanged = true;
		}
	}

	// Don't need to replicate AttachmentReplication if the root component replicates, because it already handles it.
	if (Actor->GetRootComponent() && !Actor->GetRootComponent()->GetIsReplicated())
	{
		const FRepAttachment& RepAttachment = Actor->GetAttachmentReplication();
		unrealpb::FRepAttachment* RepAttachmentFullState = FullState->mutable_attachmentreplication();
		unrealpb::FRepAttachment* RepAttachmentDeltaState = DeltaState->mutable_attachmentreplication();
		if (RepAttachment.AttachParent != ChanneldUtils::GetObjectByRef(RepAttachmentFullState->mutable_attachparent(), Actor->GetWorld(), false))
		{
			RepAttachmentDeltaState->mutable_attachparent()->CopyFrom(ChanneldUtils::GetRefOfObject(RepAttachment.AttachParent, Actor->GetNetConnection()));
			bStateChanged = true;
		}
		if (RepAttachment.AttachComponent != ChanneldUtils::GetActorComponentByRef<USceneComponent>(RepAttachmentFullState->mutable_attachcomponent(), Actor->GetWorld()))
		{
			RepAttachmentDeltaState->mutable_attachcomponent()->CopyFrom(ChanneldUtils::GetRefOfActorComponent(RepAttachment.AttachComponent, Actor->GetNetConnection()));
			bStateChanged = true;
		}
		if (ChanneldUtils::SetIfNotSame(RepAttachmentFullState->mutable_locationoffset(), RepAttachment.LocationOffset))
		{
			ChanneldUtils::SetIfNotSame(RepAttachmentDeltaState->mutable_locationoffset(), RepAttachment.LocationOffset);
			bStateChanged = true;
		}
		if (ChanneldUtils::SetIfNotSame(RepAttachmentFullState->mutable_relativescale(), RepAttachment.RelativeScale3D))
		{
			ChanneldUtils::SetIfNotSame(RepAttachmentDeltaState->mutable_relativescale(), RepAttachment.RelativeScale3D);
			bStateChanged = true;
		}
		if (ChanneldUtils::SetIfNotSame(RepAttachmentFullState->mutable_rotationoffset(), RepAttachment.RotationOffset))
		{
			ChanneldUtils::SetIfNotSame(RepAttachmentDeltaState->mutable_rotationoffset(), RepAttachment.RotationOffset);
			bStateChanged = true;
		}
	}

	FullState->MergeFrom(*DeltaState);
}

void FChanneldActorReplicator::OnStateChanged(const google::protobuf::Message* InNewState)
{
	if (!Actor.IsValid())
	{
		return;
	}

	if (Actor->GetLocalRole() > ENetRole::ROLE_SimulatedProxy)
	{
		return;
	}

	auto NewState = static_cast<const unrealpb::ActorState*>(InNewState);
	FullState->MergeFrom(*NewState);
	bStateChanged = false;

	if (NewState->has_breplicatemovement())
	{
		Actor->SetReplicateMovement(NewState->breplicatemovement());
		Actor->OnRep_ReplicateMovement();
	}
	
	// Client reverses the local/remote role as the role in the dedicated server.
	if (NewState->has_localrole())
	{
		// FIXME: handle the case of SimulatedProxy in DS
		if (Actor->IsNetMode(NM_DedicatedServer))
		{
			Actor->SetRole((ENetRole)NewState->localrole());
		}
		else
		{
			*RemoteRolePtr = (uint8)NewState->localrole();
		}
	}
	if (NewState->has_remoterole())
	{
		if (Actor->IsNetMode(NM_DedicatedServer))
		{
			*RemoteRolePtr = (uint8)NewState->remoterole();
		}
		else
		{
			Actor->SetRole((ENetRole)NewState->remoterole());
		}
	}
	/*
	*/
	if (NewState->has_owningconnid())
	{
		UChanneldConnection* ConnToChanneld = GEngine->GetEngineSubsystem<UChanneldConnection>();
		if (ConnToChanneld->GetConnId() == NewState->owningconnid())
		{
			Actor->SetRole(ROLE_AutonomousProxy);
		}
		else if (Actor->GetLocalRole() == ROLE_AutonomousProxy)
		{
			Actor->SetRole(ROLE_SimulatedProxy);
		}
		const static UEnum* Enum = StaticEnum<ENetRole>();
		UE_LOG(LogChanneld, Log, TEXT("[Client] Updated actor %s's role from %s to %s, local/remote owning connId: %d/%d"),
			*Actor->GetName(),
			*Enum->GetNameStringByValue(Actor->GetLocalRole()),
			*Enum->GetNameStringByValue(Actor->GetLocalRole()),
			ConnToChanneld->GetConnId(),
			NewState->owningconnid()
		);
	}

	if (NewState->has_owner())
	{
		// Special case: the client won't create other player's controller. Pawn and PlayerState's owner is PlayerController.
		if (Actor->HasAuthority() || (!Actor->IsA<APawn>() && !Actor->IsA<APlayerState>()))
		{
			// TODO: handle unmapped NetGUID
			Actor->SetOwner(Cast<AActor>(ChanneldUtils::GetObjectByRef(&NewState->owner(), Actor->GetWorld())));
			Actor->ProcessEvent(OnRep_OwnerFunc, NULL);
		}
	}
	if (NewState->has_bhidden())
	{
		Actor->SetHidden(NewState->bhidden());
	}
	if (NewState->has_btearoff())
	{
		*bTearOffPtr = NewState->btearoff();
	}
	if (NewState->has_bcanbedamaged())
	{
		Actor->SetCanBeDamaged(NewState->bcanbedamaged());
	}
	if (NewState->has_instigator())
	{
		// TODO: handle unmapped NetGUID
		Actor->SetInstigator(Cast<APawn>(ChanneldUtils::GetObjectByRef(&NewState->instigator(), Actor->GetWorld())));
		Actor->OnRep_Instigator();
	}

	if (NewState->has_replicatedmovement())
	{
		if (NewState->replicatedmovement().has_linearvelocity())
		{
			ReplicatedMovementPtr->LinearVelocity = ChanneldUtils::GetVector(NewState->replicatedmovement().linearvelocity());
		}
		if (NewState->replicatedmovement().has_angularvelocity())
		{
			ReplicatedMovementPtr->AngularVelocity = ChanneldUtils::GetVector(NewState->replicatedmovement().angularvelocity());
		}
		if (NewState->replicatedmovement().has_location())
		{
			ReplicatedMovementPtr->Location = ChanneldUtils::GetVector(NewState->replicatedmovement().location());
		}
		if (NewState->replicatedmovement().has_rotation())
		{
			ReplicatedMovementPtr->Rotation = ChanneldUtils::GetRotator(NewState->replicatedmovement().rotation());
		}
		if (NewState->replicatedmovement().has_bsimulatedphysicsleep())
		{
			ReplicatedMovementPtr->bSimulatedPhysicSleep = NewState->replicatedmovement().bsimulatedphysicsleep();
		}
		if (NewState->replicatedmovement().has_brepphysics())
		{
			ReplicatedMovementPtr->bRepPhysics = NewState->replicatedmovement().brepphysics();
		}

		Actor->ProcessEvent(OnRep_ReplicatedMovementFunc, NULL);
	}
}

