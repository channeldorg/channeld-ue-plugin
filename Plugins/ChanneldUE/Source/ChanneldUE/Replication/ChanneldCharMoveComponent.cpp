// Fill out your copyright notice in the Description page of Project Settings.


#include "ChanneldCharMoveComponent.h"

#include "ChanneldUtils.h"
#include "GameFramework/Character.h"

UChanneldCharMoveComponent::UChanneldCharMoveComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetMoveResponseDataContainer(DefaultMoveResponseDataContainer);
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


bool FChanneldCharacterMoveResponseDataContainer::Serialize(
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
		if (bIsSaving)
		{
			std::string NewBaseData = ChanneldUtils::GetRefOfActorComponent(ClientAdjustment.NewBase,
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
				ClientAdjustment.NewBase = Cast<UPrimitiveComponent>(NewBase);
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
