#pragma once

#include "CoreMinimal.h"
#include "GameFramework\Character.h"
#include "ChanneldReplicatorBase.h"

class CHANNELDUE_API FChanneldCharacterReplicator_Legacy : public FChanneldReplicatorBase
{
public:
	FChanneldCharacterReplicator_Legacy(UObject* InTargetObj);
	virtual ~FChanneldCharacterReplicator_Legacy() override;

	//~Begin FChanneldReplicatorBase Interface
	virtual UClass* GetTargetClass() override { return ACharacter::StaticClass(); }
	virtual google::protobuf::Message* GetDeltaState() override;
	virtual void ClearState() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnStateChanged(const google::protobuf::Message* NewState) override;
	//~End FChanneldReplicatorBase Interface

	virtual TSharedPtr<google::protobuf::Message> SerializeFunctionParams(UFunction* Func, void* Params, FOutParmRec* OutParams, bool& bSuccess) override;
	virtual TSharedPtr<void> DeserializeFunctionParams(UFunction* Func, const std::string& ParamsPayload, bool& bSuccess, bool& bDeferredRPC) override;

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
