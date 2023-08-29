#include "ChanneldCharacterReplicator.h"
#include "Net/UnrealNetwork.h"
#include "Engine/PackageMapClient.h"
#include "ChanneldReplicationComponent.h"
#include "ChanneldUtils.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/CharacterMovementComponent.h"

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

	
	// ~Begin copy of ACharacter::PreReplication
	if (Character->GetCharacterMovement()->CurrentRootMotion.HasActiveRootMotionSources() || Character->IsPlayingNetworkedRootMotionMontage())
	{
		const FAnimMontageInstance* RootMotionMontageInstance = Character->GetRootMotionAnimMontageInstance();

		Character->RepRootMotion.bIsActive = true;
		// Is position stored in local space?
		Character->RepRootMotion.bRelativePosition = Character->GetBasedMovement().HasRelativeLocation();
		Character->RepRootMotion.bRelativeRotation = Character->GetBasedMovement().HasRelativeRotation();
		Character->RepRootMotion.Location			= Character->RepRootMotion.bRelativePosition ? Character->GetBasedMovement().Location : FVector_NetQuantize100(FRepMovement::RebaseOntoZeroOrigin(Character->GetActorLocation(), Character->GetWorld()->OriginLocation));
		Character->RepRootMotion.Rotation			= Character->RepRootMotion.bRelativeRotation ? Character->GetBasedMovement().Rotation : Character->GetActorRotation();
		Character->RepRootMotion.MovementBase		= Character->GetBasedMovement().MovementBase;
		Character->RepRootMotion.MovementBaseBoneName = Character->GetBasedMovement().BoneName;
		if (RootMotionMontageInstance)
		{
			Character->RepRootMotion.AnimMontage		= RootMotionMontageInstance->Montage;
			Character->RepRootMotion.Position			= RootMotionMontageInstance->GetPosition();
		}
		else
		{
			Character->RepRootMotion.AnimMontage = nullptr;
		}

		Character->RepRootMotion.AuthoritativeRootMotion = Character->GetCharacterMovement()->CurrentRootMotion;
		Character->RepRootMotion.Acceleration = Character->GetCharacterMovement()->GetCurrentAcceleration();
		Character->RepRootMotion.LinearVelocity = Character->GetCharacterMovement()->Velocity;
	}
	else
	{
		Character->RepRootMotion.Clear();
	}

	Character->bProxyIsJumpForceApplied = (Character->JumpForceTimeRemaining > 0.0f);
	*MovementModeValuePtr = Character->GetCharacterMovement()->PackNetworkMovementMode();	
	*BasedMovementValuePtr = Character->GetBasedMovement();

	// Optimization: only update and replicate these values if they are actually going to be used.
	if (Character->GetBasedMovement().HasRelativeLocation())
	{
		// When velocity becomes zero, force replication so the position is updated to match the server (it may have moved due to simulation on the client).
		BasedMovementValuePtr->bServerHasVelocity = !Character->GetCharacterMovement()->Velocity.IsZero();

		// Make sure absolute rotations are updated in case rotation occurred after the base info was saved.
		if (!Character->GetBasedMovement().HasRelativeRotation())
		{
			BasedMovementValuePtr->Rotation = Character->GetActorRotation();
		}
	}

	// Save bandwidth by not replicating this value unless it is necessary, since it changes every update.
	if ((Character->GetCharacterMovement()->NetworkSmoothingMode == ENetworkSmoothingMode::Linear) || Character->GetCharacterMovement()->bNetworkAlwaysReplicateTransformUpdateTimestamp)
	{
		*ServerLastTransformUpdateTimeStampValuePtr = Character->GetCharacterMovement()->GetServerLastTransformUpdateTimeStamp();
	}
	else
	{
		*ServerLastTransformUpdateTimeStampValuePtr = 0.f;
	}
	// ~End copy of ACharacter::PreReplication
	
	
	// TODO: RootMotion

	bool bMovementInfoChanged = false;
	unrealpb::FBasedMovementInfo* MovementInfo = FullState->mutable_basedmovement();
	unrealpb::FBasedMovementInfo* MovementInfoDelta = DeltaState->mutable_basedmovement();
	bool bUnmapped = false;
	auto OldMovementBase = Cast<UPrimitiveComponent>(ChanneldUtils::GetActorComponentByRefChecked(MovementInfo->mutable_movementbase(), Character->GetWorld(), bUnmapped, false));
	if (!bUnmapped && OldMovementBase != Character->GetMovementBase())
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

	if (ChanneldUtils::CheckDifference(Character->GetBasedMovement().Location, MovementInfo->mutable_location()))
	{
		ChanneldUtils::SetVectorToPB(MovementInfoDelta->mutable_location(), Character->GetBasedMovement().Location, MovementInfo->mutable_location());
		bStateChanged = true;
	}

	if (ChanneldUtils::CheckDifference(Character->GetBasedMovement().Rotation, MovementInfo->mutable_rotation()))
	{
		ChanneldUtils::SetRotatorToPB(MovementInfoDelta->mutable_rotation(), Character->GetBasedMovement().Rotation, MovementInfo->mutable_rotation());
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

	if (bStateChanged)
	{
		FullState->MergeFrom(*DeltaState);
	}
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
			/* We should not create the movement base here
			UChanneldNetConnection* ClientConn = nullptr;
			// Special case: server handover
			if (Character->GetWorld()->IsServer())
			{
				ClientConn = Cast<UChanneldNetConnection>(Character->GetNetConnection());
			}
			*/
			bool bUnmapped = false;
			auto NewBase = ChanneldUtils::GetActorComponentByRefChecked(&NewState->basedmovement().movementbase(), Character->GetWorld(), bUnmapped, false);
			if (!bUnmapped)
			{
				BasedMovement.MovementBase = Cast<UPrimitiveComponent>(NewBase);
			}
		}
		if (NewState->basedmovement().has_bonename())
		{
			BasedMovement.BoneName = NewState->basedmovement().bonename().c_str();
		}
		if (NewState->basedmovement().has_location())
		{
			ChanneldUtils::SetVectorFromPB(BasedMovement.Location, NewState->basedmovement().location());
		}
		if (NewState->basedmovement().has_rotation())
		{
			ChanneldUtils::SetRotatorFromPB(BasedMovement.Rotation, NewState->basedmovement().rotation());
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
	}

	if (NewState->has_serverlasttransformupdatetimestamp() && !FMath::IsNearlyEqual(NewState->serverlasttransformupdatetimestamp(), Character->GetReplicatedServerLastTransformUpdateTimeStamp()))
	{
		*ServerLastTransformUpdateTimeStampValuePtr = NewState->serverlasttransformupdatetimestamp();
	}

	if (NewState->has_movementmode() && NewState->movementmode() != Character->GetReplicatedMovementMode())
	{
		*MovementModeValuePtr = (uint8)NewState->movementmode();
		// Somehow, the new movement mode won' be applied after the handover (even we call ACharacter::PostNetReceive), and
		// SimulateMovement or SimulateRootMotion won't trigger when MovementMode = 0. So we apply it manually!
		Character->GetCharacterMovement()->ApplyNetworkMovementMode(*MovementModeValuePtr);
	}

	bool bIsCrouchedChanged = false;
	if (NewState->has_biscrouched() && NewState->biscrouched() != Character->bIsCrouched)
	{
		Character->bIsCrouched = NewState->biscrouched();
		bIsCrouchedChanged = true;
	}

	if (NewState->has_bproxyisjumpforceapplied() && NewState->bproxyisjumpforceapplied() != Character->bProxyIsJumpForceApplied)
	{
		Character->bProxyIsJumpForceApplied = NewState->bproxyisjumpforceapplied();
	}

	if (NewState->has_animrootmotiontranslationscale() && !FMath::IsNearlyEqual(NewState->animrootmotiontranslationscale(), Character->GetAnimRootMotionTranslationScale()))
	{
		*AnimRootMotionTranslationScaleValuePtr = NewState->animrootmotiontranslationscale();
	}

	bool bReplayLastTransformUpdateTimeStampChanged = false;
	if (NewState->has_replaylasttransformupdatetimestamp() && !FMath::IsNearlyEqual(NewState->replaylasttransformupdatetimestamp(), *ReplayLastTransformUpdateTimeStampPtr))
	{
		*ReplayLastTransformUpdateTimeStampPtr = NewState->replaylasttransformupdatetimestamp();
		bReplayLastTransformUpdateTimeStampChanged = true;
	}

	// Process the RepNotify functions after all the replicated properties are updated.
	if (NewState->has_basedmovement())
	{
		// Call the OnRep function to apply the change internally
		Character->OnRep_ReplicatedBasedMovement();
	}
	if (bIsCrouchedChanged)
	{
		Character->OnRep_IsCrouched();
	}
	if (bReplayLastTransformUpdateTimeStampChanged)
	{
		Character->OnRep_ReplayLastTransformUpdateTimeStamp();
	}
}

TSharedPtr<google::protobuf::Message> FChanneldCharacterReplicator::SerializeFunctionParams(UFunction* Func, void* Params, FOutParmRec* OutParams, bool& bSuccess)
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

TSharedPtr<void> FChanneldCharacterReplicator::DeserializeFunctionParams(UFunction* Func, const std::string& ParamsPayload, bool& bSuccess, bool& bDeferredRPC)
{
	bSuccess = true;
	if (Func->GetFName() == FName("ServerMovePacked"))
	{
		UNetConnection* NetConn = Character->GetNetConnection();
		if (!NetConn)
		{
			UE_LOG(LogChanneld, Error, TEXT("ServerMovePacked: character doesn't have the NetConnection to deserialize the params. NetId: %d"), GetNetGUID());
			bSuccess = false;
			return nullptr;
		}
		
		unrealpb::Character_ServerMovePacked_Params Msg;
		if (!Msg.ParseFromString(ParamsPayload))
		{
			UE_LOG(LogChanneld, Error, TEXT("Failed to parse Character_ServerMovePacked_Params"));
			bSuccess = false;
			return nullptr;
		}

		auto Params = MakeShared<ServerMovePackedParams>();
		bool bIDC;
		FArchive EmptyArchive;
		// Hack the package map into to the PackedBits for further deserialization. IDC = I don't care.
		if (Params->PackedBits.NetSerialize(EmptyArchive, NetConn->PackageMap, bIDC))
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
			UE_LOG(LogChanneld, Verbose, TEXT("Character doesn't have the NetConn to handle RPC 'ClientMoveResponsePacked'"));
			bDeferredRPC = true;
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