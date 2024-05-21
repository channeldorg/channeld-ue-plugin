#pragma once

#include "CoreMinimal.h"
#include "Replication/ChanneldReplicatorBase.h"
#include "ChanneldUtils.h"

class FChanneldStaticMeshComponentReplicator : public FChanneldReplicatorBase
{
public:
	FChanneldStaticMeshComponentReplicator(UObject* InTargetObj);
	virtual ~FChanneldStaticMeshComponentReplicator() override;

	//~Begin FChanneldReplicatorBase Interface
	virtual uint32 GetNetGUID() override;
	virtual UClass* GetTargetClass() override { return UStaticMeshComponent::StaticClass(); }
	virtual google::protobuf::Message* GetDeltaState() override;
	virtual void ClearState() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnStateChanged(const google::protobuf::Message* NewState) override;
	//~End FChanneldReplicatorBase Interface


protected:
	TWeakObjectPtr<UStaticMeshComponent> Comp;

	// [Server+Client] The accumulated channel data of the target object
	unrealpb::StaticMeshComponentState* FullState;
	// [Server] The accumulated delta change before next send
	unrealpb::StaticMeshComponentState* DeltaState;

private:
	UObject** StaticMeshPtr;
	UFunction* OnRep_StaicMeshFunc;
};
