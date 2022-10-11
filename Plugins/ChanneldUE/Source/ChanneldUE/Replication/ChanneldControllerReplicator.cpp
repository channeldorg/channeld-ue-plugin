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

	auto PlayerState = Cast<APlayerState>(ChanneldUtils::GetObjectByRef(FullState->mutable_playerstate(), Controller->GetWorld()));
	if (PlayerState != Controller->PlayerState)
	{
		DeltaState->mutable_playerstate()->MergeFrom(ChanneldUtils::GetRefOfObject(Controller->PlayerState));
		bStateChanged = true;
	}
	auto Pawn = Cast<APawn>(ChanneldUtils::GetObjectByRef(FullState->mutable_pawn(), Controller->GetWorld()));
	if (Pawn != Controller->GetPawn())
	{
		DeltaState->mutable_pawn()->MergeFrom(ChanneldUtils::GetRefOfObject(Controller->GetPawn()));
		bStateChanged = true;
	}

	FullState->MergeFrom(*DeltaState);
}

void FChanneldControllerReplicator::OnStateChanged(const google::protobuf::Message* InNewState)
{
	if (!Controller.IsValid())
	{
		return;
	}

	auto NewState = static_cast<const unrealpb::ControllerState*>(InNewState);
	FullState->MergeFrom(*NewState);
	bStateChanged = false;

	if (NewState->has_playerstate())
	{
		Controller->PlayerState = Cast<APlayerState>(ChanneldUtils::GetObjectByRef(&NewState->playerstate(), Controller->GetWorld()));
		Controller->OnRep_PlayerState();
	}

	if (NewState->has_pawn())
	{
		Controller->SetPawnFromRep(Cast<APawn>(ChanneldUtils::GetObjectByRef(&NewState->pawn(), Controller->GetWorld())));
	}
}

TSharedPtr<google::protobuf::Message> FChanneldControllerReplicator::SerializeFunctionParams(UFunction* Func, void* Params, bool& bSuccess)
{
	bSuccess = true;
	if (Func->GetFName() == FName("ClientSetLocation"))
	{
		ClientSetLocationParams* TypedParams = (ClientSetLocationParams*)Params;
		auto Msg = MakeShared<unrealpb::Controller_ClientSetLocation_Params>();
		ChanneldUtils::SetIfNotSame(Msg->mutable_newlocation(), TypedParams->NewLocation);
		ChanneldUtils::SetIfNotSame(Msg->mutable_newrotation(), TypedParams->NewRotation.Vector());
		return Msg;
	}
	else if (Func->GetFName() == FName("ClientSetRotation"))
	{
		ClientSetRotationParams* TypedParams = (ClientSetRotationParams*)Params;
		auto Msg = MakeShared<unrealpb::Controller_ClientSetRotation_Params>();
		ChanneldUtils::SetIfNotSame(Msg->mutable_newrotation(), TypedParams->NewRotation.Vector());
		Msg->set_bresetcamera(TypedParams->bResetCamera);
		return Msg;
	}

	bSuccess = false;
	return nullptr;
}

void* FChanneldControllerReplicator::DeserializeFunctionParams(UFunction* Func, const std::string& ParamsPayload, bool& bSuccess, bool& bDelayRPC)
{
	bSuccess = true;
	if (Func->GetFName() == FName("ClientSetLocation"))
	{
		unrealpb::Controller_ClientSetLocation_Params Msg;
		Msg.ParseFromString(ParamsPayload);
		auto Params = MakeShared<ClientSetLocationParams>();
		Params->NewLocation = ChanneldUtils::GetVector(Msg.newlocation());
		Params->NewRotation = ChanneldUtils::GetRotator(Msg.newrotation());
		return &Params.Get();
	}
	else if (Func->GetFName() == FName("ClientSetRotation"))
	{
		unrealpb::Controller_ClientSetRotation_Params Msg;
		Msg.ParseFromString(ParamsPayload);
		auto Params = MakeShared<ClientSetRotationParams>();
		Params->NewRotation = ChanneldUtils::GetRotator(Msg.newrotation());
		Params->bResetCamera = Msg.bresetcamera();
		return &Params.Get();
	}

	bSuccess = false;
	return nullptr;
}
