#pragma once

#include "CoreMinimal.h"
#include "ChannelDataProvider.h"
#include "GameFramework\Character.h"
#include "ChanneldReplicatorBase.h"
#include "GameFramework/HUD.h"

class CHANNELDUE_API FChanneldPlayerControllerReplicator : public FChanneldReplicatorBase
{
public:
	FChanneldPlayerControllerReplicator(UObject* InTargetObj);
	virtual ~FChanneldPlayerControllerReplicator();

	//~Begin FChanneldReplicatorBase Interface
	virtual google::protobuf::Message* GetDeltaState() override;
	virtual void ClearState() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnStateChanged(const google::protobuf::Message* NewState) override;
	//~End FChanneldReplicatorBase Interface

	virtual TSharedPtr<google::protobuf::Message> SerializeFunctionParams(UFunction* Func, void* Params) override;
	virtual void* DeserializeFunctionParams(UFunction* Func, const std::string& ParamsPayload) override;

protected:
	TWeakObjectPtr<APlayerController> PC;

	unrealpb::PlayerControllerState* FullState;
	unrealpb::PlayerControllerState* DeltaState;

private:

	// [Client] Reflection pointer to set the value back to Character
	FVector* SpawnLocationPtr;

	struct ServerUpdateCameraParams
	{
		FVector_NetQuantize CamLoc;
		int32 CamPitchAndYaw;
	};

	struct ClientSetHUDParams
	{
		TSubclassOf<AHUD> NewHUDClass;
	};

	struct ClientSetViewTargetParams
	{
		AActor* A;
		FViewTargetTransitionParams TransitionParams;
	};

	struct ClientEnableNetworkVoiceParams
	{
		bool bEnable;
	};

	struct ClientCapBandwidthParams
	{
		int32 Cap;
	};

	struct ClientRestartParams
	{
		APawn* Pawn;
	};

	struct ClientSetCameraModeParams
	{
		FName NewCamMode;
	};

	struct ClientSetRotationParams
	{
		FRotator NewRotation;
		bool bResetCamera;
	};

	struct ClientRetryClientRestartParams
	{
		APawn* Pawn;
	};

};
