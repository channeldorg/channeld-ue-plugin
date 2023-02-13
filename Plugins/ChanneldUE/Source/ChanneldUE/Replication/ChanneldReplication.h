#pragma once
#include "CoreMinimal.h"
#include "ChanneldTypes.h"
#include "UObject/SoftObjectPtr.h"

class FChanneldReplicatorBase;

typedef TFunction<FChanneldReplicatorBase*(UObject*)> FReplicatorCreateFunc;

namespace ChanneldReplication
{
	extern TMap<const UClass*, const FReplicatorCreateFunc> ReplicatorRegistry;
	extern TMap<const FString, const FReplicatorCreateFunc> BPReplicatorRegistry;

	CHANNELDUE_API void RegisterReplicator(const UClass* TargetClass, const FReplicatorCreateFunc& Func, bool Override = true);
	CHANNELDUE_API void RegisterReplicator(const FString& PathName, const FReplicatorCreateFunc& Func, bool Override = true);
	CHANNELDUE_API TArray<FChanneldReplicatorBase*> FindAndCreateReplicators(UObject* ReplicatedObj);
}

#define REGISTER_REPLICATOR_BASE(ReplicatorClass, TargetClass, Override) \
	ChanneldReplication::RegisterReplicator(TargetClass::StaticClass(), [](UObject* InTargetObj){ return new ReplicatorClass(CastChecked<TargetClass>(InTargetObj)); }, Override)

#define REGISTER_REPLICATOR(ReplicatorClass, TargetClass) \
	REGISTER_REPLICATOR_BASE(ReplicatorClass, TargetClass, true)

#define REGISTER_REPLICATOR_BP_BASE(ReplicatorClass, BlueprintPathName, Override) \
	ChanneldReplication::RegisterReplicator(BlueprintPathName, [](UObject* InTargetObj){ return new ReplicatorClass(InTargetObj, BlueprintPathName); }, Override)

#define REGISTER_REPLICATOR_BP(ReplicatorClass, BlueprintPathName) \
	REGISTER_REPLICATOR_BP_BASE(ReplicatorClass, BlueprintPathName, true)
