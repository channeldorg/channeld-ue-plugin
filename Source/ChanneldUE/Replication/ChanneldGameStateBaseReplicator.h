#pragma once

#include "CoreMinimal.h"
#include "ChanneldReplicatorBase.h"
#include "GameFramework/GameStateBase.h"

class CHANNELDUE_API FChanneldGameStateBaseReplicator : public FChanneldReplicatorBase
{
public:
	FChanneldGameStateBaseReplicator(UObject* InTargetObj);
	virtual ~FChanneldGameStateBaseReplicator() override;

	//~Begin FChanneldReplicatorBase Interface
	virtual UClass* GetTargetClass() override { return AGameStateBase::StaticClass(); }
	virtual uint32 GetNetGUID() override;
	virtual google::protobuf::Message* GetDeltaState() override;
	virtual void ClearState() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnStateChanged(const google::protobuf::Message* NewState) override;
	//~End FChanneldReplicatorBase Interface

protected:
	TWeakObjectPtr<AGameStateBase> GameStateBase;

	// [Server+Client] The accumulated channel data of the target object
	unrealpb::GameStateBase* FullState;
	// [Server] The accumulated delta change before next send
	unrealpb::GameStateBase* DeltaState;

private:
	float* ReplicatedWorldTimeSecondsPtr;
	bool* bReplicatedHasBegunPlayPtr;
	UFunction* OnRep_ReplicatedWorldTimeSecondsFunc;
	UFunction* OnRep_ReplicatedHasBegunPlayFunc;
};