#pragma once
#include "CoreMinimal.h"
#include "ChanneldTypes.h"

class FChanneldReplicatorBase;

typedef TFunction<FChanneldReplicatorBase* (UObject*)> FReplicatorCreateFunc;

//class CHANNELDUE_API ChanneldReplication
namespace ChanneldReplication
{
	//namespace
	//{
	extern TMap<const UClass*, const FReplicatorCreateFunc> ReplicatorRegistry;
	//}

	extern void RegisterReplicator(const UClass* TargetClass, const FReplicatorCreateFunc& Func);

	extern FChanneldReplicatorBase* FindAndCreateReplicator(UObject* ReplicatedObj);
}

#define REGISTER_REPLICATOR(ReplicatorClass, TargetClass) \
	ChanneldReplication::RegisterReplicator(TargetClass::StaticClass(), [](UObject* InTargetObj){ return new ReplicatorClass(CastChecked<##TargetClass>(InTargetObj)); })

