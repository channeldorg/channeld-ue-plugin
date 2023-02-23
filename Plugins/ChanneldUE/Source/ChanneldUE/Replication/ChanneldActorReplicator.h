#pragma once

#include "CoreMinimal.h"
#include "ChanneldReplicatorBase.h"
#include "ChanneldTypes.h"
#include "unreal_common.pb.h"
#include "GameFramework/Actor.h"

class CHANNELDUE_API FChanneldActorReplicator : public FChanneldReplicatorBase
{
public:
	FChanneldActorReplicator(UObject* InTargetObj);
	virtual ~FChanneldActorReplicator() override;

	//~Begin FChanneldReplicatorBase Interface
	virtual UClass* GetTargetClass() override { return AActor::StaticClass(); }
	virtual google::protobuf::Message* GetDeltaState() override;
	virtual void ClearState() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnStateChanged(const google::protobuf::Message* NewState) override;
	//~End FChanneldReplicatorBase Interface

protected:
	TWeakObjectPtr<AActor> Actor;

	// [Server+Client] The accumulated channel data of the target object
	unrealpb::ActorState* FullState;
	// [Server] The accumulated delta change before next send
	unrealpb::ActorState* DeltaState;

private:
	uint8* RemoteRolePtr;
	bool* bTearOffPtr;
	FRepMovement* ReplicatedMovementPtr;
	FRepAttachment* AttachmentReplicationPtr;

	UFunction* OnRep_OwnerFunc;
	UFunction* OnRep_ReplicatedMovementFunc;
};