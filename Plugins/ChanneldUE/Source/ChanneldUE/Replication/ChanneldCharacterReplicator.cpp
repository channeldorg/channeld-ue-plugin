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

	State = new unrealpb::CharacterState;
	MovementInfo = new unrealpb::BasedMovementInfo;

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
	delete State;
	delete MovementInfo;
}

google::protobuf::Message* FChanneldCharacterReplicator::GetState()
{
	return State;
}

void FChanneldCharacterReplicator::ClearState()
{
	bStateChanged = false;
	State->release_basedmovement();
}

void FChanneldCharacterReplicator::Tick(float DeltaTime)
{
	if (!Character.IsValid())
	{
		return;
	}

	if (!Character->HasAuthority())
	{
		return;
	}

	// TODO: RootMotion

	bool bMovementInfoChanged = false;
	unrealpb::BasedMovementInfo MovementInfoDelta;
	if (ChanneldUtils::GetObjectByRef(MovementInfo->mutable_movementbase(), Character->GetWorld()) != Character->GetMovementBase())
	{
		MovementInfoDelta.mutable_movementbase()->MergeFrom(ChanneldUtils::GetRefOfObject(Character->GetMovementBase()));
		bMovementInfoChanged = true;
	}
	
	if (FName(MovementInfo->mutable_bonename()->c_str()) != Character->GetBasedMovement().BoneName)
	{
		*MovementInfoDelta.mutable_bonename() = TCHAR_TO_UTF8(*Character->GetBasedMovement().BoneName.ToString());
		bMovementInfoChanged = true;
	}

	if (ChanneldUtils::SetIfNotSame(MovementInfo->mutable_location(), Character->GetBasedMovement().Location))
	{
		ChanneldUtils::SetIfNotSame(MovementInfoDelta.mutable_location(), Character->GetBasedMovement().Location);
		bMovementInfoChanged = true;
	}

	if (ChanneldUtils::SetIfNotSame(MovementInfo->mutable_rotation(), Character->GetBasedMovement().Rotation.Vector()))
	{
		ChanneldUtils::SetIfNotSame(MovementInfoDelta.mutable_rotation(), Character->GetBasedMovement().Rotation.Vector());
		bMovementInfoChanged = true;
	}

	if (MovementInfo->bserverhasbasecomponent() != Character->GetBasedMovement().bServerHasBaseComponent)
	{
		MovementInfoDelta.set_bserverhasbasecomponent(Character->GetBasedMovement().bServerHasBaseComponent);
		bMovementInfoChanged = true;
	}

	if (MovementInfo->brelativerotation() != Character->GetBasedMovement().bRelativeRotation)
	{
		MovementInfoDelta.set_brelativerotation(Character->GetBasedMovement().bRelativeRotation);
		bMovementInfoChanged = true;
	}

	if (MovementInfo->bserverhasvelocity() != Character->GetBasedMovement().bServerHasVelocity)
	{
		MovementInfoDelta.set_bserverhasvelocity(Character->GetBasedMovement().bServerHasVelocity);
		bMovementInfoChanged = true;
	}

	if (bMovementInfoChanged)
	{
		MovementInfo->MergeFrom(MovementInfoDelta);
		State->mutable_basedmovement()->MergeFrom(MovementInfoDelta);
		bStateChanged = true;
	}

	unrealpb::CharacterState StateDelta;
	if (!FMath::IsNearlyEqual(State->serverlasttransformupdatetimestamp(), Character->GetReplicatedServerLastTransformUpdateTimeStamp()))
	{
		StateDelta.set_serverlasttransformupdatetimestamp(Character->GetReplicatedServerLastTransformUpdateTimeStamp());
		bStateChanged = true;
	}

	if (State->movementmode() != Character->GetReplicatedMovementMode())
	{
		StateDelta.set_movementmode(Character->GetReplicatedMovementMode());
		bStateChanged = true;
	}

	if (State->biscrouched() != Character->bIsCrouched)
	{
		StateDelta.set_biscrouched(Character->bIsCrouched);
		bStateChanged = true;
	}

	if (State->bproxyisjumpforceapplied() != Character->bProxyIsJumpForceApplied)
	{
		StateDelta.set_bproxyisjumpforceapplied(Character->bProxyIsJumpForceApplied);
		bStateChanged = true;
	}

	if (!FMath::IsNearlyEqual(State->animrootmotiontranslationscale(), Character->GetAnimRootMotionTranslationScale()))
	{
		StateDelta.set_animrootmotiontranslationscale(Character->GetAnimRootMotionTranslationScale());
		bStateChanged = true;
	}

	if (!FMath::IsNearlyEqual(State->replaylasttransformupdatetimestamp(), *ReplayLastTransformUpdateTimeStampPtr))
	{
		StateDelta.set_replaylasttransformupdatetimestamp(*ReplayLastTransformUpdateTimeStampPtr);
		bStateChanged = true;
	}

	State->MergeFrom(StateDelta);
}

void FChanneldCharacterReplicator::OnStateChanged(const google::protobuf::Message* NewState)
{
	if (!Character.IsValid())
	{
		return;
	}

	// All replicated properties in Character are COND_SimulatedOnly
	if (Character->GetLocalRole() != ROLE_SimulatedProxy)
	{
		return;
	}

	auto CharacterState = static_cast<const unrealpb::CharacterState*>(NewState);

	// TODO: RootMotion

	if (CharacterState->has_basedmovement())
	{
		FBasedMovementInfo BasedMovement = Character->GetBasedMovement();
		if (CharacterState->basedmovement().has_movementbase())
		{
			BasedMovement.MovementBase = Cast<UPrimitiveComponent>(ChanneldUtils::GetObjectByRef(&CharacterState->basedmovement().movementbase(), Character->GetWorld()));
		}
		if (CharacterState->basedmovement().bonename().length() > 0)
		{
			BasedMovement.BoneName = CharacterState->basedmovement().bonename().c_str();
		}
		if (CharacterState->basedmovement().has_location())
		{
			BasedMovement.Location = FVector_NetQuantize100(ChanneldUtils::GetVector(CharacterState->basedmovement().location()));
		}
		if (CharacterState->basedmovement().has_rotation())
		{
			BasedMovement.Rotation = ChanneldUtils::GetRotator(CharacterState->basedmovement().rotation());
		}
		if (CharacterState->basedmovement().bserverhasbasecomponent() != State->mutable_basedmovement()->bserverhasbasecomponent())
		{
			BasedMovement.bServerHasBaseComponent = CharacterState->basedmovement().bserverhasbasecomponent();
		}
		if (CharacterState->basedmovement().brelativerotation() != State->mutable_basedmovement()->brelativerotation())
		{
			BasedMovement.bRelativeRotation = CharacterState->basedmovement().brelativerotation();
		}
		if (CharacterState->basedmovement().bserverhasvelocity() != State->mutable_basedmovement()->bserverhasvelocity())
		{
			BasedMovement.bServerHasVelocity = CharacterState->basedmovement().bserverhasvelocity();
		}

		// Set the protected field via Reflection
		if (BasedMovementValuePtr)
		{
			*BasedMovementValuePtr = BasedMovement;
		}
		// Call the OnRep function to apply the change internally
		Character->OnRep_ReplicatedBasedMovement();
	}

	State->MergeFrom(*NewState);

	if (!FMath::IsNearlyEqual(State->serverlasttransformupdatetimestamp(), Character->GetReplicatedServerLastTransformUpdateTimeStamp()))
	{
		*ServerLastTransformUpdateTimeStampValuePtr = State->serverlasttransformupdatetimestamp();
	}

	if (State->movementmode() != Character->GetReplicatedMovementMode())
	{
		*MovementModeValuePtr = (uint8)State->movementmode();
	}

	if (State->biscrouched() != Character->bIsCrouched)
	{
		Character->bIsCrouched = State->biscrouched();
		Character->OnRep_IsCrouched();
	}

	if (State->bproxyisjumpforceapplied() != Character->bProxyIsJumpForceApplied)
	{
		Character->bProxyIsJumpForceApplied = State->bproxyisjumpforceapplied();
	}

	if (!FMath::IsNearlyEqual(State->animrootmotiontranslationscale(), Character->GetAnimRootMotionTranslationScale()))
	{
		*AnimRootMotionTranslationScaleValuePtr = State->animrootmotiontranslationscale();
	}

	if (!FMath::IsNearlyEqual(State->replaylasttransformupdatetimestamp(), *ReplayLastTransformUpdateTimeStampPtr))
	{
		*ReplayLastTransformUpdateTimeStampPtr = State->replaylasttransformupdatetimestamp();
		Character->OnRep_ReplayLastTransformUpdateTimeStamp();
	}
}

TSharedPtr<google::protobuf::Message> FChanneldCharacterReplicator::SerializeFunctionParams(UFunction* Func, void* Params)
{
	if (Func->GetFName() == FName("ServerMovePacked"))
	{
		ServerMovePackedParams* TypedParams = (ServerMovePackedParams*)Params;
		char* Data = (char*)TypedParams->PackedBits.DataBits.GetData();
		auto Msg = MakeShared<unrealpb::Character_ServerMovePacked_Params>();
		Msg->set_packedbits(Data, FMath::DivideAndRoundUp(TypedParams->PackedBits.DataBits.Num(), 8));
		return Msg;
	}
	else if (Func->GetFName() == FName("ClientMoveResponsePacked"))
	{
		ClientMoveResponsePackedParams* TypedParams = (ClientMoveResponsePackedParams*)Params;
		char* Data = (char*)TypedParams->PackedBits.DataBits.GetData();
		auto Msg = MakeShared<unrealpb::Character_ClientMoveResponsePacked_Params>();
		Msg->set_packedbits(Data, FMath::DivideAndRoundUp(TypedParams->PackedBits.DataBits.Num(), 8));
		return Msg;
	}
	return nullptr;
}

void* FChanneldCharacterReplicator::DeserializeFunctionParams(UFunction* Func, const std::string& ParamsPayload)
{
	if (Func->GetFName() == FName("ServerMovePacked"))
	{
		unrealpb::Character_ServerMovePacked_Params Msg;
		Msg.ParseFromString(ParamsPayload);
		auto Params = MakeShared<ServerMovePackedParams>();
		bool bIDC;
		FArchive EmptyArchive;
		// Hack the package map into to the PackedBits for further deserialization. IDC = I don't care.
		Params->PackedBits.NetSerialize(EmptyArchive, Character->GetNetConnection()->PackageMap, bIDC);

		// ----------------------------------------------------
		// UCharacterMovementComponent::CallServerMovePacked
		// ----------------------------------------------------
		Params->PackedBits.DataBits.SetNumUninitialized(Msg.packedbits().size()*8);
		FMemory::Memcpy(Params->PackedBits.DataBits.GetData(), &Msg.packedbits(), Msg.packedbits().size());

		return &Params.Get();
	}
	else if (Func->GetFName() == FName("ClientMoveResponsePacked"))
	{
		unrealpb::Character_ClientMoveResponsePacked_Params Msg;
		Msg.ParseFromString(ParamsPayload);
		auto Params = MakeShared<ClientMoveResponsePackedParams>();
		bool bIDC;
		FArchive EmptyArchive;
		// Hack the package map into to the PackedBits for further deserialization. IDC = I don't care.
		Params->PackedBits.NetSerialize(EmptyArchive, Character->GetNetConnection()->PackageMap, bIDC);

		// ----------------------------------------------------
		// UCharacterMovementComponent::ServerSendMoveResponse
		// ----------------------------------------------------
		Params->PackedBits.DataBits.SetNumUninitialized(Msg.packedbits().size() * 8);
		FMemory::Memcpy(Params->PackedBits.DataBits.GetData(), &Msg.packedbits(), Msg.packedbits().size());

		return &Params.Get();
	}
	return nullptr;
}