#pragma once

#include "CoreMinimal.h"
#include "ChannelDataProvider.h"
#include "GameFramework\Character.h"
#include "ChanneldReplicatorBase.h"

class CHANNELDUE_API FChanneldCharacterReplicator : public FChanneldReplicatorBase
{
public:
	FChanneldCharacterReplicator(UObject* InTargetObj);
	virtual ~FChanneldCharacterReplicator();

	//~Begin FChanneldReplicatorBase Interface
	virtual google::protobuf::Message* GetDeltaState() override;
	virtual void ClearState() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnStateChanged(const google::protobuf::Message* NewState) override;
	//~End FChanneldReplicatorBase Interface

	virtual TSharedPtr<google::protobuf::Message> SerializeFunctionParams(UFunction* Func, void* Params, bool& bSuccess) override;
	virtual void* DeserializeFunctionParams(UFunction* Func, const std::string& ParamsPayload, bool& bSuccess, bool& bDelayRPC) override;

protected:
	TWeakObjectPtr<ACharacter> Character;

	// [Server+Client] The accumulated channel data of the target object
	unrealpb::CharacterState* FullState;
	// [Server] The accumulated delta change before next send
	unrealpb::CharacterState* DeltaState;

	// [Client] Reflection pointer to set the value back to Character
	FBasedMovementInfo* BasedMovementValuePtr;
	float* ServerLastTransformUpdateTimeStampValuePtr;
	uint8* MovementModeValuePtr;
	float* AnimRootMotionTranslationScaleValuePtr;
	float* ReplayLastTransformUpdateTimeStampPtr;

private:

	struct ServerMovePackedParams
	{
		FCharacterServerMovePackedBits PackedBits;
	};

	struct ClientMoveResponsePackedParams
	{
		FCharacterMoveResponsePackedBits PackedBits;
	};

};
