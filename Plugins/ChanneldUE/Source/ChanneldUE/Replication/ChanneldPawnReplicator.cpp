#include "ChanneldPawnReplicator.h"
#include "Net/UnrealNetwork.h"
#include "ChanneldUtils.h"

FChanneldPawnReplicator::FChanneldPawnReplicator(UObject* InTargetObj) : FChanneldReplicatorBase(InTargetObj)
{
	Pawn = CastChecked<APawn>(InTargetObj);
	// Remove the registered DOREP() properties in the Pawn
	TArray<FLifetimeProperty> RepProps;
	DisableAllReplicatedPropertiesOfClass(InTargetObj->GetClass(), APawn::StaticClass(), EFieldIteratorFlags::ExcludeSuper, RepProps);

	FullState = new unrealpb::PawnState;
	DeltaState = new unrealpb::PawnState;
	
	// Prepare Reflection pointers
	{
		auto Property = CastFieldChecked<const FObjectProperty>(Pawn->GetClass()->FindPropertyByName(FName("PlayerState")));
		PlayerStatePtr = Property->ContainerPtrToValuePtr<APlayerState*>(Pawn.Get());
		check(PlayerStatePtr);
	}
}

FChanneldPawnReplicator::~FChanneldPawnReplicator()
{
	delete FullState;
	delete DeltaState;
}

google::protobuf::Message* FChanneldPawnReplicator::GetDeltaState()
{
	return DeltaState;
}

void FChanneldPawnReplicator::ClearState()
{
	DeltaState->Clear();
	bStateChanged = false;
}

void FChanneldPawnReplicator::Tick(float DeltaTime)
{
	if (!Pawn.IsValid())
	{
		return;
	}

	// Only server can update channel data
	if (!Pawn->HasAuthority())
	{
		return;
	}

	// ~Begin copy of APawn::PreReplication
	if (Pawn->GetController())
	{
		Pawn->SetRemoteViewPitch(Pawn->GetController()->GetControlRotation().Pitch);
	}	
	// ~End copy of APawn::PreReplication

	auto PlayerState = Cast<APlayerState>(ChanneldUtils::GetObjectByRef(FullState->mutable_playerstate(), Pawn->GetWorld(), false));
	if (PlayerState != *PlayerStatePtr)
	{
		DeltaState->mutable_playerstate()->CopyFrom(ChanneldUtils::GetRefOfObject(*PlayerStatePtr));
		bStateChanged = true;
	}

	auto Controller = Cast<AController>(ChanneldUtils::GetObjectByRef(FullState->mutable_controller(), Pawn->GetWorld(), false));
	if (Controller != Pawn->Controller)
	{
		DeltaState->mutable_controller()->CopyFrom(ChanneldUtils::GetRefOfObject(Pawn->Controller));
		bStateChanged = true;
	}

	if (FullState->remoteviewpitch() != Pawn->RemoteViewPitch)
	{
		DeltaState->set_remoteviewpitch(Pawn->RemoteViewPitch);
		bStateChanged = true;
	}

	if (bStateChanged)
	{
		FullState->MergeFrom(*DeltaState);
	}
}

void FChanneldPawnReplicator::OnStateChanged(const google::protobuf::Message* InNewState)
{
	if (!Pawn.IsValid())
	{
		return;
	}

	/* Authority can be changed in the middle of a ChannelDataUpdate (in FChanneldActorReplicator::OnStateChanged),
	* causing PlayerState and PlayerController failed to set.
	*/
	if (Pawn->HasAuthority())
	{
		return;
	}

	auto NewState = static_cast<const unrealpb::PawnState*>(InNewState);
	FullState->MergeFrom(*NewState);
	bStateChanged = false;

	if (NewState->has_playerstate())
	{
		*PlayerStatePtr = Cast<APlayerState>(ChanneldUtils::GetObjectByRef(&NewState->playerstate(), Pawn->GetWorld()));
		Pawn->OnRep_PlayerState();
		UE_LOG(LogChanneld, Verbose, TEXT("Replicator set Pawn's PlayerState to %s"), *GetNameSafe(*PlayerStatePtr));
	}

	if (NewState->has_controller())
	{
		Pawn->Controller = Cast<AController>(ChanneldUtils::GetObjectByRef(&NewState->controller(), Pawn->GetWorld()));
		Pawn->OnRep_Controller();
		UE_LOG(LogChanneld, Verbose, TEXT("Replicator set Pawn's Controller to %s"), *GetNameSafe(Pawn->Controller));
	}

	if (NewState->has_remoteviewpitch())
	{
		Pawn->RemoteViewPitch = NewState->remoteviewpitch();
	}
}