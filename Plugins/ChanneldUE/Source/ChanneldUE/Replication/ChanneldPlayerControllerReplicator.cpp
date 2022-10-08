#include "ChanneldPlayerControllerReplicator.h"
#include "Net/UnrealNetwork.h"
#include "Engine/PackageMapClient.h"
#include "ChanneldReplicationComponent.h"
#include "ChanneldUtils.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/NetSerialization.h"

FChanneldPlayerControllerReplicator::FChanneldPlayerControllerReplicator(UObject* InTargetObj) : FChanneldReplicatorBase(InTargetObj)
{
	PC = CastChecked<APlayerController>(InTargetObj);
	/*
	*/
	// Remove the registered DOREP() properties in the Character
	TArray<FLifetimeProperty> RepProps;
	DisableAllReplicatedPropertiesOfClass(InTargetObj->GetClass(), APlayerController::StaticClass(), EFieldIteratorFlags::ExcludeSuper, RepProps);

	FullState = new unrealpb::PlayerControllerState;
	DeltaState = new unrealpb::PlayerControllerState;

	// Prepare Reflection pointers
	{
		auto Property = CastFieldChecked<const FStructProperty>(PC->GetClass()->FindPropertyByName(FName("SpawnLocation")));
		SpawnLocationPtr = Property->ContainerPtrToValuePtr<FVector>(PC.Get());
		check(SpawnLocationPtr);
	}
}

FChanneldPlayerControllerReplicator::~FChanneldPlayerControllerReplicator()
{
	delete FullState;
	delete DeltaState;
}

google::protobuf::Message* FChanneldPlayerControllerReplicator::GetDeltaState()
{
	return DeltaState;
}

void FChanneldPlayerControllerReplicator::ClearState()
{
	DeltaState->Clear();
	bStateChanged = false;
}

void FChanneldPlayerControllerReplicator::Tick(float DeltaTime)
{
	if (!PC.IsValid())
	{
		return;
	}

	if (!PC->HasAuthority())
	{
		return;
	}

	// TODO: All replicated properties in PC are COND_OwnerOnly

}

void FChanneldPlayerControllerReplicator::OnStateChanged(const google::protobuf::Message* NewState)
{
	if (!PC.IsValid())
	{
		return;
	}

	//auto CharacterState = static_cast<const unrealpb::PlayerControllerState*>(NewState);

	FullState->MergeFrom(*NewState);
}

TSharedPtr<google::protobuf::Message> FChanneldPlayerControllerReplicator::SerializeFunctionParams(UFunction* Func, void* Params, bool& bSuccess)
{
	bSuccess = true;
	if (Func->GetFName() == FName("ServerUpdateCamera"))
	{
		ServerUpdateCameraParams* TypedParams = (ServerUpdateCameraParams*)Params;
		auto Msg = MakeShared<unrealpb::PlayerController_ServerUpdateCamera_Params>();
		ChanneldUtils::SetIfNotSame(Msg->mutable_camloc(), TypedParams->CamLoc);
		Msg->set_campitchandyaw(TypedParams->CamPitchAndYaw);
		return Msg;
	}
	else if (Func->GetFName() == FName("ClientSetHUD"))
	{
		ClientSetHUDParams* TypedParams = (ClientSetHUDParams*)Params;
		auto Msg = MakeShared<unrealpb::PlayerController_ClientSetHUD_Params>();
		if (TypedParams->NewHUDClass)
		{
			Msg->set_hudclassname(std::string(TCHAR_TO_UTF8(*TypedParams->NewHUDClass->GetName())));
		}
		return Msg;
	}
	else if (Func->GetFName() == FName("ClientSetViewTarget"))
	{
		ClientSetViewTargetParams* TypedParams = (ClientSetViewTargetParams*)Params;
		auto Msg = MakeShared<unrealpb::PlayerController_ClientSetViewTarget_Params>();
		Msg->mutable_actor()->MergeFrom(ChanneldUtils::GetRefOfObject(TypedParams->A));
		Msg->set_blendtime(TypedParams->TransitionParams.BlendTime);
		Msg->set_blendfunction(TypedParams->TransitionParams.BlendFunction);
		Msg->set_blendexp(TypedParams->TransitionParams.BlendExp);
		Msg->set_blockoutgoing(TypedParams->TransitionParams.bLockOutgoing);
		return Msg;
	}
	else if (Func->GetFName() == FName("ClientEnableNetworkVoice"))
	{
		ClientEnableNetworkVoiceParams* TypedParams = (ClientEnableNetworkVoiceParams*)Params;
		auto Msg = MakeShared<unrealpb::PlayerController_ClientEnableNetworkVoice_Params>();
		Msg->set_benable(TypedParams->bEnable);
		return Msg;
	}
	else if (Func->GetFName() == FName("ClientCapBandwidth"))
	{
		ClientCapBandwidthParams* TypedParams = (ClientCapBandwidthParams*)Params;
		auto Msg = MakeShared<unrealpb::PlayerController_ClientCapBandwidth_Params>();
		Msg->set_cap(TypedParams->Cap);
		return Msg;
	}
	else if (Func->GetFName() == FName("ClientRestart"))
	{
		ClientRestartParams* TypedParams = (ClientRestartParams*)Params;
		auto Msg = MakeShared<unrealpb::PlayerController_ClientRestart_Params>();
		Msg->mutable_pawn()->MergeFrom(ChanneldUtils::GetRefOfObject(TypedParams->Pawn));
		return Msg;
	}
	else if (Func->GetFName() == FName("ClientSetCameraMode"))
	{
		ClientSetCameraModeParams* TypedParams = (ClientSetCameraModeParams*)Params;
		auto Msg = MakeShared<unrealpb::PlayerController_ClientSetCameraMode_Params>();
		Msg->set_newcammode(std::string(TCHAR_TO_UTF8(*TypedParams->NewCamMode.ToString())));
		return Msg;
	}
	else if (Func->GetFName() == FName("ClientRetryClientRestart"))
	{
		ClientRetryClientRestartParams* TypedParams = (ClientRetryClientRestartParams*)Params;
		auto Msg = MakeShared<unrealpb::PlayerController_ClientRetryClientRestart_Params>();
		Msg->mutable_pawn()->MergeFrom(ChanneldUtils::GetRefOfObject(TypedParams->Pawn));
		return Msg;
	}
	else if (Func->GetFName() == FName("ServerSetSpectatorLocation"))
	{
		ServerSetSpectatorLocationParams* TypedParams = (ServerSetSpectatorLocationParams*)Params;
		auto Msg = MakeShared<unrealpb::PlayerController_ServerSetSpectatorLocation_Params>();
		ChanneldUtils::SetIfNotSame(Msg->mutable_newloc(), TypedParams->NewLoc);
		ChanneldUtils::SetIfNotSame(Msg->mutable_newrot(), TypedParams->NewRot.Vector());
		return Msg;
	}
	else if (NoParamFunctions.Contains(Func->GetFName()))
	{
		// No need to serialize anything
		return nullptr;
	}

	bSuccess = false;
	return nullptr;
}

void* FChanneldPlayerControllerReplicator::DeserializeFunctionParams(UFunction* Func, const std::string& ParamsPayload, bool& bSuccess)
{
	bSuccess = true;
	if (Func->GetFName() == FName("ServerUpdateCamera"))
	{
		unrealpb::PlayerController_ServerUpdateCamera_Params Msg;
		Msg.ParseFromString(ParamsPayload);
		auto Params = MakeShared<ServerUpdateCameraParams>();
		Params->CamLoc = FVector_NetQuantize(ChanneldUtils::GetVector(Msg.camloc()));
		Params->CamPitchAndYaw = Msg.campitchandyaw();
		return &Params.Get();
	}
	else if (Func->GetFName() == FName("ClientSetHUD"))
	{
		unrealpb::PlayerController_ClientSetHUD_Params Msg;
		Msg.ParseFromString(ParamsPayload);
		auto Params = MakeShared<ClientSetHUDParams>();
		if (Msg.has_hudclassname())
		{
			Params->NewHUDClass = LoadClass<AHUD>(NULL, UTF8_TO_TCHAR(Msg.hudclassname().c_str()));
		}
		return &Params.Get();
	}
	else if (Func->GetFName() == FName("ClientSetViewTarget"))
	{
		unrealpb::PlayerController_ClientSetViewTarget_Params Msg;
		Msg.ParseFromString(ParamsPayload);
		auto Params = MakeShared<ClientSetViewTargetParams>();
		// Parameter "A" can be null
		Params->A = Cast<AActor>(ChanneldUtils::GetObjectByRef(&Msg.actor(), PC->GetWorld()));
		Params->TransitionParams.BlendTime = Msg.blendtime();
		Params->TransitionParams.BlendFunction = TEnumAsByte<enum EViewTargetBlendFunction>((uint8)Msg.blendfunction());
		Params->TransitionParams.BlendExp = Msg.blendexp();
		Params->TransitionParams.bLockOutgoing = (uint32)Msg.blockoutgoing();
		return &Params.Get();
	}
	else if (Func->GetFName() == FName("ClientEnableNetworkVoice"))
	{
		unrealpb::PlayerController_ClientEnableNetworkVoice_Params Msg;
		Msg.ParseFromString(ParamsPayload);
		auto Params = MakeShared<ClientEnableNetworkVoiceParams>();
		Params->bEnable = Msg.benable();
		return &Params.Get();
	}
	else if (Func->GetFName() == FName("ClientCapBandwidth"))
	{
		unrealpb::PlayerController_ClientCapBandwidth_Params Msg;
		Msg.ParseFromString(ParamsPayload);
		auto Params = MakeShared<ClientCapBandwidthParams>();
		Params->Cap = Msg.cap();
		return &Params.Get();
	}
	else if (Func->GetFName() == FName("ClientRestart"))
	{
		unrealpb::PlayerController_ClientRestart_Params Msg;
		Msg.ParseFromString(ParamsPayload);
		auto Params = MakeShared<ClientRestartParams>();
		// Parameter "NewPawn" can be null
		Params->Pawn = Cast<APawn>(ChanneldUtils::GetObjectByRef(&Msg.pawn(), PC->GetWorld()));
		return &Params.Get();
	}
	else if (Func->GetFName() == FName("ClientSetCameraMode"))
	{
		unrealpb::PlayerController_ClientSetCameraMode_Params Msg;
		Msg.ParseFromString(ParamsPayload);
		auto Params = MakeShared<ClientSetCameraModeParams>();
		Params->NewCamMode = FName(UTF8_TO_TCHAR(Msg.newcammode().c_str()));
		return &Params.Get();
	}
	else if (Func->GetFName() == FName("ClientRetryClientRestart"))
	{
		unrealpb::PlayerController_ClientRetryClientRestart_Params Msg;
		Msg.ParseFromString(ParamsPayload);
		auto Params = MakeShared<ClientRetryClientRestartParams>();
		// Parameter "NewPawn" can be null
		Params->Pawn = Cast<APawn>(ChanneldUtils::GetObjectByRef(&Msg.pawn(), PC->GetWorld()));
		return &Params.Get();
	}
	else if (Func->GetFName() == FName("ServerSetSpectatorLocation"))
	{
		unrealpb::PlayerController_ServerSetSpectatorLocation_Params Msg;
		Msg.ParseFromString(ParamsPayload);
		auto Params = MakeShared<ServerSetSpectatorLocationParams>();
		Params->NewLoc = ChanneldUtils::GetVector(Msg.newloc());
		Params->NewRot = ChanneldUtils::GetRotator(Msg.newrot());
		return &Params.Get();
	}
	else if (NoParamFunctions.Contains(Func->GetFName()))
	{
		// No need to deserialize anything
		return nullptr;
	}

	bSuccess = false;
	return nullptr;
}