#include "ChanneldControllerReplicator.h"
#include "Net/UnrealNetwork.h"
#include "ChanneldUtils.h"
#include "GameFramework/PlayerState.h"

FChanneldControllerReplicator::FChanneldControllerReplicator(UObject* InTargetObj) : FChanneldReplicatorBase(InTargetObj)
{
	Controller = CastChecked<AController>(InTargetObj);

	// Remove the registered DOREP() properties in the Character
	TArray<FLifetimeProperty> RepProps;
	DisableAllReplicatedPropertiesOfClass(InTargetObj->GetClass(), AController::StaticClass(), EFieldIteratorFlags::ExcludeSuper, RepProps);

	FullState = new unrealpb::ControllerState;
	DeltaState = new unrealpb::ControllerState;
}

FChanneldControllerReplicator::~FChanneldControllerReplicator()
{
	delete FullState;
	delete DeltaState;
}

google::protobuf::Message* FChanneldControllerReplicator::GetDeltaState()
{
	return DeltaState;
}

void FChanneldControllerReplicator::ClearState()
{
	DeltaState->Clear();
	bStateChanged = false;
}

void FChanneldControllerReplicator::Tick(float DeltaTime)
{
	if (!Controller.IsValid())
	{
		return;
	}

	if (!Controller->HasAuthority())
	{
		return;
	}

	bool bUnmapped = false;
	auto PlayerState = Cast<APlayerState>(ChanneldUtils::GetObjectByRef(FullState->mutable_playerstate(), Controller->GetWorld(), bUnmapped, false));
	if (!bUnmapped && PlayerState != Controller->PlayerState)
	{
		// Only set the PlayerState if it's replicated
		if (Controller->PlayerState == nullptr || Controller->PlayerState->GetIsReplicated())
		{
			DeltaState->mutable_playerstate()->CopyFrom(*ChanneldUtils::GetRefOfObject(Controller->PlayerState));
			bStateChanged = true;
		}
	}

	bUnmapped = false;
	auto Pawn = Cast<APawn>(ChanneldUtils::GetObjectByRef(FullState->mutable_pawn(), Controller->GetWorld(), bUnmapped, false));
	if (!bUnmapped && Pawn != Controller->GetPawn())
	{
		// Only set the Pawn if it's replicated
		if (Controller->GetPawn() == nullptr || Controller->GetPawn()->GetIsReplicated())
		{
			DeltaState->mutable_pawn()->CopyFrom(*ChanneldUtils::GetRefOfObject(Controller->GetPawn()));
			bStateChanged = true;
		}
	}

	if (bStateChanged)
	{
		FullState->MergeFrom(*DeltaState);
	}
}

void FChanneldControllerReplicator::OnStateChanged(const google::protobuf::Message* InNewState)
{
	if (!Controller.IsValid())
	{
		return;
	}

	/* Authority can be changed in the middle of a ChannelDataUpdate (in FChanneldActorReplicator::OnStateChanged),
	 * causing PlayerState and Pawn failed to set.
	*/
	if (Controller->HasAuthority())
	{
		return;
	}

	auto NewState = static_cast<const unrealpb::ControllerState*>(InNewState);
	FullState->MergeFrom(*NewState);
	bStateChanged = false;

	if (NewState->has_playerstate())
	{
		Controller->PlayerState = Cast<APlayerState>(ChanneldUtils::GetObjectByRef(&NewState->playerstate(), Controller->GetWorld()));
		UE_LOG(LogChanneld, Verbose, TEXT("Replicator set Controller's PlayerState to %s"), *GetNameSafe(Controller->PlayerState));
	}

	if (NewState->has_pawn())
	{
		Controller->SetPawnFromRep(Cast<APawn>(ChanneldUtils::GetObjectByRef(&NewState->pawn(), Controller->GetWorld())));
		UE_LOG(LogChanneld, Verbose, TEXT("Replicator set Controller's Pawn to %s"), *GetNameSafe(Controller->GetPawn()));
	}
	
	if (NewState->has_playerstate())
	{
		Controller->OnRep_PlayerState();
	}
}

TSharedPtr<google::protobuf::Message> FChanneldControllerReplicator::SerializeFunctionParams(UFunction* Func, void* Params, FOutParmRec* OutParams, bool& bSuccess)
{
	bSuccess = true;
	if (Func->GetFName() == FName("ClientSetLocation"))
	{
		ClientSetLocationParams* TypedParams = (ClientSetLocationParams*)Params;
		auto Msg = MakeShared<unrealpb::Controller_ClientSetLocation_Params>();
		ChanneldUtils::SetVectorToPB(Msg->mutable_newlocation(), TypedParams->NewLocation);
		ChanneldUtils::SetRotatorToPB(Msg->mutable_newrotation(), TypedParams->NewRotation);
		return Msg;
	}
	else if (Func->GetFName() == FName("ClientSetRotation"))
	{
		ClientSetRotationParams* TypedParams = (ClientSetRotationParams*)Params;
		auto Msg = MakeShared<unrealpb::Controller_ClientSetRotation_Params>();
		ChanneldUtils::SetRotatorToPB(Msg->mutable_newrotation(), TypedParams->NewRotation);
		Msg->set_bresetcamera(TypedParams->bResetCamera);
		return Msg;
	}

	bSuccess = false;
	return nullptr;
}

TSharedPtr<void> FChanneldControllerReplicator::DeserializeFunctionParams(UFunction* Func, const std::string& ParamsPayload, bool& bSuccess, bool& bDeferredRPC)
{
	bSuccess = true;
	if (Func->GetFName() == FName("ClientSetLocation"))
	{
		unrealpb::Controller_ClientSetLocation_Params Msg;
		Msg.ParseFromString(ParamsPayload);
		auto Params = MakeShared<ClientSetLocationParams>();
		ChanneldUtils::SetVectorFromPB(Params->NewLocation, Msg.newlocation());
		ChanneldUtils::SetRotatorFromPB(Params->NewRotation, Msg.newrotation());
		return Params;
	}
	else if (Func->GetFName() == FName("ClientSetRotation"))
	{
		unrealpb::Controller_ClientSetRotation_Params Msg;
		Msg.ParseFromString(ParamsPayload);
		auto Params = MakeShared<ClientSetRotationParams>();
		ChanneldUtils::SetRotatorFromPB(Params->NewRotation, Msg.newrotation());
		Params->bResetCamera = Msg.bresetcamera();
		return Params;
	}

	bSuccess = false;
	return nullptr;
}
