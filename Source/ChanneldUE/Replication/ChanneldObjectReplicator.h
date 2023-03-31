#pragma once

#include "CoreMinimal.h"
#include "ChanneldReplicatorBase.h"
#include "unreal_common.pb.h"

class CHANNELDUE_API FChanneldObjectReplicator : public FChanneldReplicatorBase
{
public:

	FChanneldObjectReplicator(UObject* InTargetObj);
	
	//~Begin FChanneldReplicatorBase Interface
	virtual UClass* GetTargetClass() override { return UObject::StaticClass(); }
	virtual google::protobuf::Message* GetDeltaState() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnStateChanged(const google::protobuf::Message* NewState) override;
	//~End FChanneldReplicatorBase Interface

protected:
	unrealpb::UnrealObjectRef ObjRef;
};