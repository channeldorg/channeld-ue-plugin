#pragma once

#include "CoreMinimal.h"
#include "ChanneldReplicatorBase.h"
#include "google/protobuf/message.h"
#include "GameFramework/Controller.h"

class CHANNELDUE_API FChanneldControllerReplicator : public FChanneldReplicatorBase
{
public:
	FChanneldControllerReplicator(UObject* InTargetObj);
	virtual ~FChanneldControllerReplicator() override;

	//~Begin FChanneldReplicatorBase Interface
	virtual UClass* GetTargetClass() override { return AController::StaticClass(); }
	virtual google::protobuf::Message* GetDeltaState() override;
	virtual void ClearState() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnStateChanged(const google::protobuf::Message* NewState) override;
	//~End FChanneldReplicatorBase Interface

	virtual TSharedPtr<google::protobuf::Message> SerializeFunctionParams(UFunction* Func, void* Params, bool& bSuccess) override;
	virtual TSharedPtr<void> DeserializeFunctionParams(UFunction* Func, const std::string& ParamsPayload, bool& bSuccess, bool& bDeferredRPC) override;

protected:
	TWeakObjectPtr<AController> Controller;

	unrealpb::ControllerState* FullState;
	unrealpb::ControllerState* DeltaState;

private:

	struct ClientSetLocationParams
	{
		FVector NewLocation;
		FRotator NewRotation;
	};

	struct ClientSetRotationParams
	{
		FRotator NewRotation;
		bool bResetCamera;
	};

};