#pragma once

#include "CoreMinimal.h"
#include "ChannelDataProvider.h"
#include "ChanneldTypes.h"
#include "GameFramework\Character.h"
#include "ChanneldReplicatorBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/LocalMessage.h"
#include "GameFramework/PlayerState.h"

class CHANNELDUE_API FChanneldPlayerControllerReplicator : public FChanneldReplicatorBase
{
public:
	FChanneldPlayerControllerReplicator(UObject* InTargetObj);
	virtual ~FChanneldPlayerControllerReplicator();

	//~Begin FChanneldReplicatorBase Interface
	virtual UClass* GetTargetClass() { return APlayerController::StaticClass(); }
	virtual google::protobuf::Message* GetDeltaState() override;
	virtual void ClearState() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnStateChanged(const google::protobuf::Message* NewState) override;
	//~End FChanneldReplicatorBase Interface

	virtual TSharedPtr<google::protobuf::Message> SerializeFunctionParams(UFunction* Func, void* Params, bool& bSuccess) override;
	virtual TSharedPtr<void> DeserializeFunctionParams(UFunction* Func, const std::string& ParamsPayload, bool& bSuccess, bool& bDelayRPC) override;

protected:
	TWeakObjectPtr<APlayerController> PC;

	unrealpb::PlayerControllerState* FullState;
	unrealpb::PlayerControllerState* DeltaState;

	const TMap<FName, void*> NoParamFunctions
	{
		{FName("ClientVoiceHandshakeComplete"), nullptr},
		{FName("ServerVerifyViewTarget"), nullptr},
		{FName("ServerCheckClientPossessionReliable"), nullptr},
		{FName("ServerShortTimeout"), nullptr},
	};

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

	struct ClientRetryClientRestartParams
	{
		APawn* Pawn;
	};

	struct ServerSetSpectatorLocationParams
	{
		FVector NewLoc;
		FRotator NewRot;
	};

	struct ServerAcknowledgePossessionParams
	{
		APawn* Pawn;
	};

	struct ClientGotoStateParams
	{
		FName NewState;
	};

	struct ClientReceiveLocalizedMessageParams
	{
		TSubclassOf<ULocalMessage> Message;
		int32 Switch;
		APlayerState* RelatedPlayerState_1;
		APlayerState* RelatedPlayerState_2;
		UObject* OptionalObject;
	};
};
