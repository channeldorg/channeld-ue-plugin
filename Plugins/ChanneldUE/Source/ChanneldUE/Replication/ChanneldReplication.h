#pragma once
#include "CoreMinimal.h"
#include "ChanneldTypes.h"
#include "UObject/SoftObjectPtr.h"

class FChanneldReplicatorBase;

typedef TFunction<FChanneldReplicatorBase* (UObject*)> FReplicatorCreateFunc;

namespace ChanneldReplication
{
	extern TMap<const UClass*, const FReplicatorCreateFunc> ReplicatorRegistry;

	CHANNELDUE_API void RegisterReplicator(const UClass* TargetClass, const FReplicatorCreateFunc& Func);
	CHANNELDUE_API TArray<FChanneldReplicatorBase*> FindAndCreateReplicators(UObject* ReplicatedObj);
}

#define REGISTER_REPLICATOR(ReplicatorClass, TargetClass) \
	ChanneldReplication::RegisterReplicator(TargetClass::StaticClass(), [](UObject* InTargetObj){ return new ReplicatorClass(CastChecked<TargetClass>(InTargetObj)); })

#define REGISTER_REPLICATOR_BP(ReplicatorClass, BlueprintClassName) \
	{ \
	UClass* TargetClass = TSoftClassPtr<UObject>(FSoftObjectPath(TEXT(BlueprintClassName))).LoadSynchronous(); \
	ChanneldReplication::RegisterReplicator(TargetClass, [TargetClass](UObject* InTargetObj){ return new ReplicatorClass(InTargetObj, TargetClass); }); \
	} \
