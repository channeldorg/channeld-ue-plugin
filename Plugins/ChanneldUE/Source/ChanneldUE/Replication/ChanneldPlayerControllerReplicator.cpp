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
	// Remove the registered DOREP() properties in the Character
	TArray<FLifetimeProperty> RepProps;
	DisableAllReplicatedPropertiesOfClass(InTargetObj->GetClass(), APlayerController::StaticClass(), EFieldIteratorFlags::ExcludeSuper, RepProps);
	*/

	State = new unrealpb::PlayerControllerState;

	// Prepare Reflection pointers
	{
		auto Property = CastFieldChecked<const FStructProperty>(PC->GetClass()->FindPropertyByName(FName("SpawnLocation")));
		SpawnLocationPtr = Property->ContainerPtrToValuePtr<FVector>(PC.Get());
		check(SpawnLocationPtr);
	}
}

FChanneldPlayerControllerReplicator::~FChanneldPlayerControllerReplicator()
{
	delete State;
}

google::protobuf::Message* FChanneldPlayerControllerReplicator::GetState()
{
	return State;
}

void FChanneldPlayerControllerReplicator::ClearState()
{
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

	State->MergeFrom(*NewState);
}

TSharedPtr<google::protobuf::Message> FChanneldPlayerControllerReplicator::SerializeFunctionParams(UFunction* Func, void* Params)
{
	if (Func->GetFName() == FName("ServerUpdateCamera"))
	{
		ServerUpdateCameraParams* TypedParams = (ServerUpdateCameraParams*)Params;
		auto Msg = MakeShared<unrealpb::PlayerController_ServerUpdateCamera_Params>();
		ChanneldUtils::SetIfNotSame(Msg->mutable_camloc(), TypedParams->CamLoc);
		Msg->set_campitchandyaw(TypedParams->CamPitchAndYaw);
		return Msg;
	}
	return nullptr;
}

void* FChanneldPlayerControllerReplicator::DeserializeFunctionParams(UFunction* Func, const std::string& ParamsPayload)
{
	if (Func->GetFName() == FName("ServerUpdateCamera"))
	{
		unrealpb::PlayerController_ServerUpdateCamera_Params Msg;
		Msg.ParseFromString(ParamsPayload);
		auto Params = MakeShared<ServerUpdateCameraParams>();
		Params->CamLoc = FVector_NetQuantize(ChanneldUtils::GetVector(Msg.camloc()));
		Params->CamPitchAndYaw = Msg.campitchandyaw();

		return &Params.Get();
	}
	return nullptr;
}