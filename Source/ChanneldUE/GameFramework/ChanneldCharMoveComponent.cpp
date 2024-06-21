#include "ChanneldCharMoveComponent.h"

#include "ChanneldUtils.h"
#include "GameFramework/Character.h"

UChanneldCharMoveComponent::UChanneldCharMoveComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// SetMoveResponseDataContainer(DefaultMoveDataContainer_SC);
	// SetNetworkMoveDataContainer(DefaultMoveDataContainer_CS);
}

bool UChanneldCharMoveComponent::ForcePositionUpdate(float DeltaTime)
{
	// Don't update if the server no longer has authority over the character (mainly caused by the handover)
	if (!CharacterOwner->HasAuthority())
	{
		return false;
	}
	return Super::ForcePositionUpdate(DeltaTime);
}


static void SerializeMovementBase(FArchive& Ar, UPrimitiveComponent*& MovementBase, UCharacterMovementComponent& CharacterMovement)
{
	if (Ar.IsSaving())
	{
		std::string NewBaseData = ChanneldUtils::GetRefOfActorComponent(MovementBase,
			CharacterMovement.GetCharacterOwner()->GetNetConnection()).SerializeAsString();
		uint32 DataSize = NewBaseData.size();
		Ar.SerializeIntPacked(DataSize);
		Ar.Serialize((void*)NewBaseData.data(), DataSize);
	}
	else
	{
		uint32 DataSize;
		Ar.SerializeIntPacked(DataSize);
		uint8* NewBaseData = new uint8[DataSize];
		Ar.Serialize(NewBaseData, DataSize);
		unrealpb::ActorComponentRef ObjRef;
		ObjRef.ParseFromArray(NewBaseData, DataSize);
		delete[] NewBaseData;
		bool bNetGUIDUnmapped;
		auto NewBase = ChanneldUtils::GetActorComponentByRefChecked(&ObjRef, CharacterMovement.GetWorld(), bNetGUIDUnmapped, false);
		if (!bNetGUIDUnmapped)
		{
			MovementBase = Cast<UPrimitiveComponent>(NewBase);
		}
		else
		{
			UE_LOG(LogChanneld, Warning, TEXT("Failed to set the NewBase for the CharacterMovement of %s, unmapped NetGUID: %d"),
				*GetNameSafe(CharacterMovement.GetOwner()), ObjRef.owner().netguid());
			/* Maybe we don't need to fail
			return false;
			*/
		}
	}
}

bool FChanneldCharacterMoveDataContainer_SC::Serialize(
	UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap)
{
	bool bLocalSuccess = true;
	const bool bIsSaving = Ar.IsSaving();

	Ar.SerializeBits(&ClientAdjustment.bAckGoodMove, 1);
	Ar << ClientAdjustment.TimeStamp;

	if (IsCorrection())
	{
		Ar.SerializeBits(&bHasBase, 1);
		Ar.SerializeBits(&bHasRotation, 1);
		Ar.SerializeBits(&bRootMotionMontageCorrection, 1);
		Ar.SerializeBits(&bRootMotionSourceCorrection, 1);

		ClientAdjustment.NewLoc.NetSerialize(Ar, PackageMap, bLocalSuccess);
		ClientAdjustment.NewVel.NetSerialize(Ar, PackageMap, bLocalSuccess);

		if (bHasRotation)
		{
			ClientAdjustment.NewRot.NetSerialize(Ar, PackageMap, bLocalSuccess);
		}
		else if (!bIsSaving)
		{
			ClientAdjustment.NewRot = FRotator::ZeroRotator;
		}

		/* Replace the NetworkGUID(InternalLoadObject) with ActorComponentRef
		SerializeOptionalValue<UPrimitiveComponent*>(bIsSaving, Ar, ClientAdjustment.NewBase, nullptr);
		*/
		SerializeMovementBase(Ar, ClientAdjustment.NewBase, CharacterMovement);
		
		SerializeOptionalValue<FName>(bIsSaving, Ar, ClientAdjustment.NewBaseBoneName, NAME_None);
		SerializeOptionalValue<uint8>(bIsSaving, Ar, ClientAdjustment.MovementMode, MOVE_Walking);
		Ar.SerializeBits(&ClientAdjustment.bBaseRelativePosition, 1);

		if (bRootMotionMontageCorrection)
		{
			Ar << RootMotionTrackPosition;
		}
		else if (!bIsSaving)
		{
			RootMotionTrackPosition = -1.0f;
		}

		if (bRootMotionSourceCorrection)
		{
			if (FRootMotionSourceGroup* RootMotionSourceGroup = GetRootMotionSourceGroup(CharacterMovement))
			{
				RootMotionSourceGroup->NetSerialize(Ar, PackageMap, bLocalSuccess);
			}
		}

		if (bRootMotionMontageCorrection || bRootMotionSourceCorrection)
		{
			RootMotionRotation.NetSerialize(Ar, PackageMap, bLocalSuccess);
		}
	}

	return !Ar.IsError();
}

bool FChanneldCharacterNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar,
	UPackageMap* PackageMap, ENetworkMoveType MoveType)
{
	NetworkMoveType = MoveType;

	bool bLocalSuccess = true;
	const bool bIsSaving = Ar.IsSaving();

	Ar << TimeStamp;

	// TODO: better packing with single bit per component indicating zero/non-zero
	Acceleration.NetSerialize(Ar, PackageMap, bLocalSuccess);

	Location.NetSerialize(Ar, PackageMap, bLocalSuccess);

	// ControlRotation : FRotator handles each component zero/non-zero test; it uses a single signal bit for zero/non-zero, and uses 16 bits per component if non-zero.
	ControlRotation.NetSerialize(Ar, PackageMap, bLocalSuccess);

	SerializeOptionalValue<uint8>(bIsSaving, Ar, CompressedMoveFlags, 0);

	if (MoveType == ENetworkMoveType::NewMove)
	{
		// Location, relative movement base, and ending movement mode is only used for error checking, so only save for the final move.

		/* Replace the NetworkGUID(InternalLoadObject) with ActorComponentRef 
		SerializeOptionalValue<UPrimitiveComponent*>(bIsSaving, Ar, MovementBase, nullptr);
		*/
		SerializeMovementBase(Ar, MovementBase, CharacterMovement);
		
		SerializeOptionalValue<FName>(bIsSaving, Ar, MovementBaseBoneName, NAME_None);
		SerializeOptionalValue<uint8>(bIsSaving, Ar, MovementMode, MOVE_Walking);
	}

	return !Ar.IsError();
}
