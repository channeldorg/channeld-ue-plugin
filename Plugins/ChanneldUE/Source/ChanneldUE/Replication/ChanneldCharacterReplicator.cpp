#include "ChanneldCharacterReplicator.h"
#include "Net/UnrealNetwork.h"
#include "Engine/PackageMapClient.h"
#include "ChanneldReplicationComponent.h"
#include "ChanneldUtils.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/NetSerialization.h"

FChanneldCharacterReplicator::FChanneldCharacterReplicator(UObject* InTargetObj) : FChanneldReplicatorBase(InTargetObj)
{
	Character = CastChecked<ACharacter>(InTargetObj);
	// Remove the registered DOREP() properties in the Character
	TArray<FLifetimeProperty> RepProps;
	DisableAllReplicatedPropertiesOfClass(InTargetObj->GetClass(), ACharacter::StaticClass(), EFieldIteratorFlags::ExcludeSuper, RepProps);

	FullState = new unrealpb::CharacterState;
	DeltaState = new unrealpb::CharacterState;

	// Prepare Reflection pointers
	{
		auto Property = CastFieldChecked<const FStructProperty>(Character->GetClass()->FindPropertyByName(FName("ReplicatedBasedMovement")));
		BasedMovementValuePtr = Property->ContainerPtrToValuePtr<FBasedMovementInfo>(Character.Get());
		check(BasedMovementValuePtr);
	}
	{
		auto Property = CastFieldChecked<const FFloatProperty>(Character->GetClass()->FindPropertyByName(FName("ReplicatedServerLastTransformUpdateTimeStamp")));
		ServerLastTransformUpdateTimeStampValuePtr = Property->ContainerPtrToValuePtr<float>(Character.Get());
		check(ServerLastTransformUpdateTimeStampValuePtr);
	}
	{
		auto Property = CastFieldChecked<const FByteProperty>(Character->GetClass()->FindPropertyByName(FName("ReplicatedMovementMode")));
		MovementModeValuePtr = Property->ContainerPtrToValuePtr<uint8>(Character.Get());
		check(MovementModeValuePtr);
	}
	{
		auto Property = CastFieldChecked<const FFloatProperty>(Character->GetClass()->FindPropertyByName(FName("AnimRootMotionTranslationScale")));
		AnimRootMotionTranslationScaleValuePtr = Property->ContainerPtrToValuePtr<float>(Character.Get());
		check(AnimRootMotionTranslationScaleValuePtr);
	}
	{
		auto Property = CastFieldChecked<const FFloatProperty>(Character->GetClass()->FindPropertyByName(FName("ReplayLastTransformUpdateTimeStamp")));
		ReplayLastTransformUpdateTimeStampPtr = Property->ContainerPtrToValuePtr<float>(Character.Get());
		check(ReplayLastTransformUpdateTimeStampPtr);
	}
}

FChanneldCharacterReplicator::~FChanneldCharacterReplicator()
{
	delete FullState;
	delete DeltaState;
}

google::protobuf::Message* FChanneldCharacterReplicator::GetDeltaState()
{
	return DeltaState;
}

void FChanneldCharacterReplicator::ClearState()
{
	DeltaState->Clear();
	bStateChanged = false;
}

void FChanneldCharacterReplicator::Tick(float DeltaTime)
{
	if (!Character.IsValid())
	{
		return;
	}

	// Only server can update channel data
	if (!Character->HasAuthority())
	{
		return;
	}

	// TODO: RootMotion

	bool bMovementInfoChanged = false;
	unrealpb::FBasedMovementInfo* MovementInfo = FullState->mutable_basedmovement();
	unrealpb::FBasedMovementInfo* MovementInfoDelta = DeltaState->mutable_basedmovement();
	auto OldMovementBase = ChanneldUtils::GetActorComponentByRef<UPrimitiveComponent>(MovementInfo->mutable_movementbase(), Character->GetWorld());
	if (OldMovementBase != Character->GetMovementBase())
	{
		uint32 OldNetGUID = MovementInfoDelta->mutable_movementbase()->mutable_owner()->netguid();
		// The movement base's Actor (e.g. the 'Floor') normally doesn't have a NetConnection. In that case, we use the character's NetConnection.
		MovementInfoDelta->mutable_movementbase()->CopyFrom(ChanneldUtils::GetRefOfActorComponent(Character->GetMovementBase(), Character->GetNetConnection()));
		bStateChanged = true;
		// UE_LOG(LogChanneld, Log, TEXT("MovementBase changed: %s -> %s, owner NetGUID: %d -> %d"), *GetNameSafe(OldMovementBase), *GetNameSafe(Character->GetMovementBase()), OldNetGUID, MovementInfoDelta->mutable_movementbase()->mutable_owner()->netguid());
	}
	
	if (FName(MovementInfo->mutable_bonename()->c_str()) != Character->GetBasedMovement().BoneName)
	{
		*MovementInfoDelta->mutable_bonename() = TCHAR_TO_UTF8(*Character->GetBasedMovement().BoneName.ToString());
		bStateChanged = true;
	}

	if (ChanneldUtils::SetIfNotSame(MovementInfo->mutable_location(), Character->GetBasedMovement().Location))
	{
		ChanneldUtils::SetIfNotSame(MovementInfoDelta->mutable_location(), Character->GetBasedMovement().Location);
		bStateChanged = true;
	}

	if (ChanneldUtils::SetIfNotSame(MovementInfo->mutable_rotation(), Character->GetBasedMovement().Rotation))
	{
		ChanneldUtils::SetIfNotSame(MovementInfoDelta->mutable_rotation(), Character->GetBasedMovement().Rotation);
		bStateChanged = true;
	}

	if (MovementInfo->bserverhasbasecomponent() != Character->GetBasedMovement().bServerHasBaseComponent)
	{
		MovementInfoDelta->set_bserverhasbasecomponent(Character->GetBasedMovement().bServerHasBaseComponent);
		bStateChanged = true;
	}

	if (MovementInfo->brelativerotation() != Character->GetBasedMovement().bRelativeRotation)
	{
		MovementInfoDelta->set_brelativerotation(Character->GetBasedMovement().bRelativeRotation);
		bStateChanged = true;
	}

	if (MovementInfo->bserverhasvelocity() != Character->GetBasedMovement().bServerHasVelocity)
	{
		MovementInfoDelta->set_bserverhasvelocity(Character->GetBasedMovement().bServerHasVelocity);
		bStateChanged = true;
	}

	if (!FMath::IsNearlyEqual(FullState->serverlasttransformupdatetimestamp(), Character->GetReplicatedServerLastTransformUpdateTimeStamp()))
	{
		DeltaState->set_serverlasttransformupdatetimestamp(Character->GetReplicatedServerLastTransformUpdateTimeStamp());
		bStateChanged = true;
	}

	if (FullState->movementmode() != Character->GetReplicatedMovementMode())
	{
		DeltaState->set_movementmode(Character->GetReplicatedMovementMode());
		bStateChanged = true;
	}

	if (FullState->biscrouched() != Character->bIsCrouched)
	{
		DeltaState->set_biscrouched(Character->bIsCrouched);
		bStateChanged = true;
	}

	if (FullState->bproxyisjumpforceapplied() != Character->bProxyIsJumpForceApplied)
	{
		DeltaState->set_bproxyisjumpforceapplied(Character->bProxyIsJumpForceApplied);
		bStateChanged = true;
	}

	if (!FMath::IsNearlyEqual(FullState->animrootmotiontranslationscale(), Character->GetAnimRootMotionTranslationScale()))
	{
		DeltaState->set_animrootmotiontranslationscale(Character->GetAnimRootMotionTranslationScale());
		bStateChanged = true;
	}

	if (!FMath::IsNearlyEqual(FullState->replaylasttransformupdatetimestamp(), *ReplayLastTransformUpdateTimeStampPtr))
	{
		DeltaState->set_replaylasttransformupdatetimestamp(*ReplayLastTransformUpdateTimeStampPtr);
		bStateChanged = true;
	}

	// TODO: Optimization: Set the FullState as well as the DeltaState above
	FullState->MergeFrom(*DeltaState);
}

void FChanneldCharacterReplicator::OnStateChanged(const google::protobuf::Message* InNewState)
{
	if (!Character.IsValid())
	{
		return;
	}

	// All replicated properties in Character are COND_SimulatedOnly
	if (Character->GetLocalRole() != ROLE_SimulatedProxy)
	{
		//UE_LOG(LogChanneld, Verbose, TEXT("Skip updating AutonomousProxy Character %s"), *Character->GetName());
		return;
	}

	auto NewState = static_cast<const unrealpb::CharacterState*>(InNewState);
	FullState->MergeFrom(*NewState);
	bStateChanged = false;

	// TODO: RootMotion

	if (NewState->has_basedmovement())
	{
		FBasedMovementInfo BasedMovement = Character->GetBasedMovement();
		if (NewState->basedmovement().has_movementbase())
		{
			BasedMovement.MovementBase = ChanneldUtils::GetActorComponentByRef<UPrimitiveComponent>(&NewState->basedmovement().movementbase(), Character->GetWorld());
		}
		if (NewState->basedmovement().has_bonename())
		{
			BasedMovement.BoneName = NewState->basedmovement().bonename().c_str();
		}
		if (NewState->basedmovement().has_location())
		{
			BasedMovement.Location = FVector_NetQuantize100(ChanneldUtils::GetVector(NewState->basedmovement().location()));
		}
		if (NewState->basedmovement().has_rotation())
		{
			BasedMovement.Rotation = ChanneldUtils::GetRotator(NewState->basedmovement().rotation());
		}
		if (NewState->basedmovement().has_bserverhasbasecomponent() && NewState->basedmovement().bserverhasbasecomponent() != BasedMovement.bServerHasBaseComponent)
		{
			BasedMovement.bServerHasBaseComponent = NewState->basedmovement().bserverhasbasecomponent();
		}
		if (NewState->basedmovement().has_brelativerotation() && NewState->basedmovement().brelativerotation() != BasedMovement.bRelativeRotation)
		{
			BasedMovement.bRelativeRotation = NewState->basedmovement().brelativerotation();
		}
		if (NewState->basedmovement().has_bserverhasvelocity() && NewState->basedmovement().bserverhasvelocity() != BasedMovement.bServerHasVelocity)
		{
			BasedMovement.bServerHasVelocity = NewState->basedmovement().bserverhasvelocity();
		}

		// Set the protected field via Reflection
		if (BasedMovementValuePtr)
		{
			*BasedMovementValuePtr = BasedMovement;
		}
		// Call the OnRep function to apply the change internally
		Character->OnRep_ReplicatedBasedMovement();
	}

	if (NewState->has_serverlasttransformupdatetimestamp() && !FMath::IsNearlyEqual(NewState->serverlasttransformupdatetimestamp(), Character->GetReplicatedServerLastTransformUpdateTimeStamp()))
	{
		*ServerLastTransformUpdateTimeStampValuePtr = NewState->serverlasttransformupdatetimestamp();
	}

	if (NewState->has_movementmode() && NewState->movementmode() != Character->GetReplicatedMovementMode())
	{
		*MovementModeValuePtr = (uint8)NewState->movementmode();
	}

	if (NewState->has_biscrouched() && NewState->biscrouched() != Character->bIsCrouched)
	{
		Character->bIsCrouched = NewState->biscrouched();
		Character->OnRep_IsCrouched();
	}

	if (NewState->has_bproxyisjumpforceapplied() && NewState->bproxyisjumpforceapplied() != Character->bProxyIsJumpForceApplied)
	{
		Character->bProxyIsJumpForceApplied = NewState->bproxyisjumpforceapplied();
	}

	if (NewState->has_animrootmotiontranslationscale() && !FMath::IsNearlyEqual(NewState->animrootmotiontranslationscale(), Character->GetAnimRootMotionTranslationScale()))
	{
		*AnimRootMotionTranslationScaleValuePtr = NewState->animrootmotiontranslationscale();
	}

	if (NewState->has_replaylasttransformupdatetimestamp() && !FMath::IsNearlyEqual(NewState->replaylasttransformupdatetimestamp(), *ReplayLastTransformUpdateTimeStampPtr))
	{
		*ReplayLastTransformUpdateTimeStampPtr = NewState->replaylasttransformupdatetimestamp();
		Character->OnRep_ReplayLastTransformUpdateTimeStamp();
	}
}

TSharedPtr<google::protobuf::Message> FChanneldCharacterReplicator::SerializeFunctionParams(UFunction* Func, void* Params, bool& bSuccess)
{
	bSuccess = true;
	if (Func->GetFName() == FName("ServerMovePacked"))
	{
		ServerMovePackedParams* TypedParams = (ServerMovePackedParams*)Params;
		char* Data = (char*)TypedParams->PackedBits.DataBits.GetData();
		auto Msg = MakeShared<unrealpb::Character_ServerMovePacked_Params>();
		Msg->set_bitsnum(TypedParams->PackedBits.DataBits.Num());
		Msg->set_packedbits(Data, FMath::DivideAndRoundUp(TypedParams->PackedBits.DataBits.Num(), 8));
		//UE_LOG(LogChanneld, Log, TEXT("Sending ServerMovePacked with PackedBits: %d, Timestamp: %d"), TypedParams->PackedBits.DataBits.Num(), *TypedParams->PackedBits.DataBits.GetData());
		return Msg;
	}
	else if (Func->GetFName() == FName("ClientMoveResponsePacked"))
	{
		ClientMoveResponsePackedParams* TypedParams = (ClientMoveResponsePackedParams*)Params;
		char* Data = (char*)TypedParams->PackedBits.DataBits.GetData();
		auto Msg = MakeShared<unrealpb::Character_ClientMoveResponsePacked_Params>();
		Msg->set_bitsnum(TypedParams->PackedBits.DataBits.Num());
		Msg->set_packedbits(Data, FMath::DivideAndRoundUp(TypedParams->PackedBits.DataBits.Num(), 8));
		return Msg;
	}

	bSuccess = false;
	return nullptr;
}

TSharedPtr<void> FChanneldCharacterReplicator::DeserializeFunctionParams(UFunction* Func, const std::string& ParamsPayload, bool& bSuccess, bool& bDelayRPC)
{
	bSuccess = true;
	if (Func->GetFName() == FName("ServerMovePacked"))
	{
		unrealpb::Character_ServerMovePacked_Params Msg;
		if (!Msg.ParseFromString(ParamsPayload))
		{
			UE_LOG(LogChanneld, Warning, TEXT("Failed to parse Character_ServerMovePacked_Params"));
			return nullptr;
		}

		auto Params = MakeShared<ServerMovePackedParams>();
		bool bIDC;
		FArchive EmptyArchive;
		// Hack the package map into to the PackedBits for further deserialization. IDC = I don't care.
		if (Params->PackedBits.NetSerialize(EmptyArchive, Character->GetNetConnection()->PackageMap, bIDC))
		{
			// ----------------------------------------------------
			// UCharacterMovementComponent::CallServerMovePacked
			// ----------------------------------------------------
			Params->PackedBits.DataBits.SetNumUninitialized(Msg.bitsnum());
			FMemory::Memcpy(Params->PackedBits.DataBits.GetData(), Msg.packedbits().data(), Msg.packedbits().size());
			//UE_LOG(LogChanneld, Log, TEXT("Received ServerMovePacked with PackedBits: %d, Timestamp: %d"), Params->PackedBits.DataBits.Num(), *Params->PackedBits.DataBits.GetData());
		}

		return Params;
	}
	else if (Func->GetFName() == FName("ClientMoveResponsePacked"))
	{
		// The character doesn't have the owning NetConnection yet. Postpone the execution of the RPC.
		UNetConnection* NetConn = Character->GetNetConnection();
		if (!NetConn)
		{
			bDelayRPC = true;
			return nullptr;
		}
		
		unrealpb::Character_ClientMoveResponsePacked_Params Msg;
		if (!Msg.ParseFromString(ParamsPayload))
		{
			UE_LOG(LogChanneld, Warning, TEXT("Failed to parse Character_ServerMovePacked_Params"));
			return nullptr;
		}

		auto Params = MakeShared<ClientMoveResponsePackedParams>();
		bool bIDC;
		FArchive EmptyArchive;
		// Hack the package map into to the PackedBits for further deserialization. IDC = I don't care.
		if (Params->PackedBits.NetSerialize(EmptyArchive, NetConn->PackageMap, bIDC))
		{
			// ----------------------------------------------------
			// UCharacterMovementComponent::ServerSendMoveResponse
			// ----------------------------------------------------
			Params->PackedBits.DataBits.SetNumUninitialized(Msg.bitsnum());
			FMemory::Memcpy(Params->PackedBits.DataBits.GetData(), Msg.packedbits().data(), Msg.packedbits().size());
		}

		return Params;
	}

	bSuccess = false;
	return nullptr;
}