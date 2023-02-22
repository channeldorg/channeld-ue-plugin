#include "ChanneldActorReplicator.h"
#include "Net/UnrealNetwork.h"
#include "ChanneldUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"

FChanneldActorReplicator::FChanneldActorReplicator(UObject* InTargetObj) : FChanneldReplicatorBase(InTargetObj)
{
	Actor = CastChecked<AActor>(InTargetObj);
	if (Actor->IsA<AGameStateBase>())
	{
		NetGUID = Channeld::GameStateNetId;
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

	// ~Begin copy of AActor::PreReplication
	AttachmentReplicationPtr->AttachParent = nullptr;
	AttachmentReplicationPtr->AttachComponent = nullptr;
	Actor->GatherCurrentMovement();
	// ~End copy of AActor::PreReplication
	
	if (Actor->IsReplicatingMovement() != FullState->breplicatemovement())
	{
		DeltaState->set_breplicatemovement(Actor->IsReplicatingMovement());
		bStateChanged = true;
	}

	bool bRoleChanged = false;
	if ((uint32)Actor->GetLocalRole() != FullState->localrole())
	{
		DeltaState->set_localrole(Actor->GetLocalRole());
		bStateChanged = true;
		bRoleChanged = true;
	}
	if ((uint32)Actor->GetRemoteRole() != FullState->remoterole())
	{
		DeltaState->set_remoterole(Actor->GetRemoteRole());
		bStateChanged = true;
		bRoleChanged = true;
	}

	auto Connnection = Cast<UChanneldNetConnection>(Actor->GetNetConnection());
	// If role changed, always send the owning connId for the client to adjust the role.
	if (IsValid(Connnection) && (bRoleChanged || Connnection->GetConnId() != FullState->owningconnid()))
	{
		DeltaState->set_owningconnid(Connnection->GetConnId());
		bStateChanged = true;
	}

	bool bOwnerUnmapped = false;
	AActor* Owner = Cast<AActor>(ChanneldUtils::GetObjectByRef(FullState->mutable_owner(), Actor->GetWorld(), bOwnerUnmapped, false));
	if (!bOwnerUnmapped && Actor->GetOwner() != Owner)
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

	bool bInstigatorUnmapped = false;
	AActor* Instigator = Cast<AActor>(ChanneldUtils::GetObjectByRef(FullState->mutable_instigator(), Actor->GetWorld(), bInstigatorUnmapped, false));
	if (!bInstigatorUnmapped && Actor->GetInstigator() != Instigator)
	{
		DeltaState->mutable_instigator()->CopyFrom(ChanneldUtils::GetRefOfObject(Actor->GetInstigator()));
		bStateChanged = true;
	}

	if (Actor->IsReplicatingMovement())
	{
		FRepMovement& RepMovement = *ReplicatedMovementPtr;
		unrealpb::FRepMovement* RepMovementFullState = FullState->mutable_replicatedmovement();
		/* Optimization: Don't create the delta state until there's a change
		unrealpb::FRepMovement* RepMovementDeltaState = DeltaState->mutable_replicatedmovement();
		*/
		if (ChanneldUtils::CheckDifference(RepMovement.LinearVelocity, RepMovementFullState->mutable_linearvelocity()))
		{
			ChanneldUtils::SetVectorToPB(DeltaState->mutable_replicatedmovement()->mutable_linearvelocity(), RepMovement.LinearVelocity, RepMovementFullState->mutable_linearvelocity());
			bStateChanged = true;
		}
		if (ChanneldUtils::CheckDifference(RepMovement.AngularVelocity, RepMovementFullState->mutable_angularvelocity()))
		{
			ChanneldUtils::SetVectorToPB(DeltaState->mutable_replicatedmovement()->mutable_angularvelocity(), RepMovement.AngularVelocity, RepMovementFullState->mutable_angularvelocity());
			bStateChanged = true;
		}
		if (ChanneldUtils::CheckDifference(RepMovement.Location, RepMovementFullState->mutable_location()))
		{
			ChanneldUtils::SetVectorToPB(DeltaState->mutable_replicatedmovement()->mutable_location(), RepMovement.Location, RepMovementFullState->mutable_location());
			bStateChanged = true;
		}
		if (ChanneldUtils::CheckDifference(RepMovement.Rotation, RepMovementFullState->mutable_rotation()))
		{
			ChanneldUtils::SetRotatorToPB(DeltaState->mutable_replicatedmovement()->mutable_rotation(), RepMovement.Rotation, RepMovementFullState->mutable_rotation());
			bStateChanged = true;
		}
		if (RepMovement.bSimulatedPhysicSleep != RepMovementFullState->bsimulatedphysicsleep())
		{
			DeltaState->mutable_replicatedmovement()->set_bsimulatedphysicsleep(RepMovement.bSimulatedPhysicSleep);
			bStateChanged = true;
		}
		if (RepMovement.bRepPhysics != RepMovementFullState->brepphysics())
		{
			DeltaState->mutable_replicatedmovement()->set_brepphysics(RepMovement.bRepPhysics);
			bStateChanged = true;
		}
	}

	// Don't need to replicate AttachmentReplication if the root component replicates, because it already handles it.
	if (Actor->GetRootComponent() && !Actor->GetRootComponent()->GetIsReplicated())
	{
		const FRepAttachment& RepAttachment = Actor->GetAttachmentReplication();
		unrealpb::FRepAttachment* RepAttachmentFullState = FullState->mutable_attachmentreplication();
		/* Optimization: Don't create the delta state until there's a change
		unrealpb::FRepAttachment* RepAttachmentDeltaState = DeltaState->mutable_attachmentreplication();
		*/
		if (RepAttachment.AttachParent != ChanneldUtils::GetObjectByRef(RepAttachmentFullState->mutable_attachparent(), Actor->GetWorld(), false))
		{
			DeltaState->mutable_attachmentreplication()->mutable_attachparent()->CopyFrom(ChanneldUtils::GetRefOfObject(RepAttachment.AttachParent, Actor->GetNetConnection()));
			bStateChanged = true;
		}
		bool bUnmapped = false;
		if (RepAttachment.AttachComponent != Cast<USceneComponent>(ChanneldUtils::GetActorComponentByRefChecked(RepAttachmentFullState->mutable_attachcomponent(), Actor->GetWorld(), bUnmapped, false)) && !bUnmapped)
		{
			DeltaState->mutable_attachmentreplication()->mutable_attachcomponent()->CopyFrom(ChanneldUtils::GetRefOfActorComponent(RepAttachment.AttachComponent, Actor->GetNetConnection()));
			bStateChanged = true;
		}
		
		if (RepAttachment.AttachParent)
		{
			if (ChanneldUtils::CheckDifference(RepAttachment.LocationOffset, RepAttachmentFullState->mutable_locationoffset()))
			{
				ChanneldUtils::SetVectorToPB(DeltaState->mutable_attachmentreplication()->mutable_locationoffset(), RepAttachment.LocationOffset, RepAttachmentFullState->mutable_locationoffset());
				bStateChanged = true;
			}
			if (ChanneldUtils::CheckDifference(RepAttachment.RelativeScale3D, RepAttachmentFullState->mutable_relativescale()))
			{
				ChanneldUtils::SetVectorToPB(DeltaState->mutable_attachmentreplication()->mutable_relativescale(), RepAttachment.RelativeScale3D, RepAttachmentFullState->mutable_relativescale());
				bStateChanged = true;
			}
			if (ChanneldUtils::CheckDifference(RepAttachment.RotationOffset, RepAttachmentFullState->mutable_rotationoffset()))
			{
				ChanneldUtils::SetRotatorToPB(DeltaState->mutable_attachmentreplication()->mutable_rotationoffset(), RepAttachment.RotationOffset, RepAttachmentFullState->mutable_rotationoffset());
				bStateChanged = true;
			}
		}
	}

	if (bStateChanged)
	{
		FullState->MergeFrom(*DeltaState);
	}
}

void FChanneldActorReplicator::OnStateChanged(const google::protobuf::Message* InNewState)
{
	if (!Actor.IsValid())
	{
		return;
	}

	// AutonomousProxy still applies the update, as it may downgrade to SimulatedProxy
	if (Actor->GetLocalRole() > ENetRole::ROLE_AutonomousProxy)
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


	if (Actor->IsNetMode(NM_DedicatedServer))
	{
		// Actor->SetRole(ChanneldUtils::ServerGetActorNetRole(Actor.Get()));
		if (NewState->has_remoterole())
		{
			*RemoteRolePtr = (uint8)NewState->remoterole();
		}
	}
	else
	{
		if (NewState->has_remoterole())
		{
			Actor->SetRole((ENetRole)NewState->remoterole());
		}
		if (NewState->has_localrole())
		{
			*RemoteRolePtr = (uint8)NewState->localrole();
		}
		if (NewState->has_owningconnid())
		{
			ChanneldUtils::SetActorRoleByOwningConnId(Actor.Get(), NewState->owningconnid());
		}
	}

	/*
	// Client reverses the local/remote role as the role in the dedicated server (except for GameStateBase).
	if (NewState->has_localrole())
	{
		if (Actor->IsNetMode(NM_DedicatedServer) && !Actor->IsA<AGameStateBase>())
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
		if (Actor->IsNetMode(NM_DedicatedServer) && !Actor->IsA<AGameStateBase>())
		{
			*RemoteRolePtr = (uint8)NewState->remoterole();
		}
		else
		{
			Actor->SetRole((ENetRole)NewState->remoterole());
		}
	}

	// Update the NetRole based on the OwningConnId (the actor's owning NetConnection's ConnId)
	if (NewState->has_owningconnid())
	{
		ChanneldUtils::SetActorRoleByOwningConnId(Actor.Get(), NewState->owningconnid());
	}
	*/

	if (NewState->has_owner())
	{
		// if (Actor->HasAuthority())
		{
			bool bNetGUIDUnmapped = false;
			/* Actor's owner should always be created before this moment.
			// Special case: the client won't create other player's controller. Pawn and PlayerState's owner is PlayerController.
			bool bCreateIfNotInCache = !Actor->IsA<APawn>() && !Actor->IsA<APlayerState>();
			*/
			UObject* Owner = ChanneldUtils::GetObjectByRef(&NewState->owner(), Actor->GetWorld(), bNetGUIDUnmapped, false);
			if (!bNetGUIDUnmapped)
			{
				Actor->SetOwner(Cast<AActor>(Owner));
				Actor->ProcessEvent(OnRep_OwnerFunc, NULL);
				UE_LOG(LogChanneld, Verbose, TEXT("Replicator set Actor's Owner to %s"), *GetNameSafe(Owner));
			}
			else
			{
				UE_LOG(LogChanneld, Warning, TEXT("ActorReplicator failed to set the owner of %s, NetId: %d"), *Actor->GetName(), NewState->owner().netguid());
			}
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
		bool bNetGUIDUnmapped = false;
		UObject* Instigator = ChanneldUtils::GetObjectByRef(&NewState->instigator(), Actor->GetWorld(), bNetGUIDUnmapped, true);
		if (!bNetGUIDUnmapped)
		{
			Actor->SetInstigator(Cast<APawn>(Instigator));
			Actor->OnRep_Instigator();
		}
		else
		{
			UE_LOG(LogChanneld, Warning, TEXT("ActorReplicator failed to set the instigator of %s, NetId: %d"), *Actor->GetName(), NewState->instigator().netguid());
		}
	}

	if (NewState->has_replicatedmovement())
	{
		if (NewState->replicatedmovement().has_linearvelocity())
		{
			ChanneldUtils::SetVectorFromPB(ReplicatedMovementPtr->LinearVelocity, NewState->replicatedmovement().linearvelocity());
		}
		if (NewState->replicatedmovement().has_angularvelocity())
		{
			ChanneldUtils::SetVectorFromPB(ReplicatedMovementPtr->AngularVelocity, NewState->replicatedmovement().angularvelocity());
		}
		if (NewState->replicatedmovement().has_location())
		{
			ChanneldUtils::SetVectorFromPB(ReplicatedMovementPtr->Location, NewState->replicatedmovement().location());
		}
		if (NewState->replicatedmovement().has_rotation())
		{
			ChanneldUtils::SetRotatorFromPB(ReplicatedMovementPtr->Rotation, NewState->replicatedmovement().rotation());
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

	// In debug builds, Actor->PostNetReceive() will throw a check error as the owner is already set.
	// Calling Actor->PreNetReceive() can bypass the check.
	Actor->PreNetReceive();
}

