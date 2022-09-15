#pragma once

#include "CoreMinimal.h"
#include "ChannelDataProvider.h"
#include "GameFramework\Character.h"
#include "ChanneldReplicatorBase.h"

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

	struct ServerUpdateCameraParams
	{
		FVector_NetQuantize CamLoc;
		int32 CamPitchAndYaw;
	};

	// [Client] Reflection pointer to set the value back to Character
	FVector* SpawnLocationPtr;

};
