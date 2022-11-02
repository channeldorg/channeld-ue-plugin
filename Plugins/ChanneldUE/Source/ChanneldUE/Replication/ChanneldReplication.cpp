#include "ChanneldReplication.h"

TMap<const UClass*, const FReplicatorCreateFunc> ChanneldReplication::ReplicatorRegistry;

void ChanneldReplication::RegisterReplicator(const UClass* TargetClass, const FReplicatorCreateFunc& Func)
{
	ReplicatorRegistry.Add(TargetClass, Func);
	UE_LOG(LogChanneld, Log, TEXT("Registered replicator for %s, registry size: %d"), *TargetClass->GetFullName(), ReplicatorRegistry.Num());
}

// TODO: use the pool
TArray<FChanneldReplicatorBase*> ChanneldReplication::FindAndCreateReplicators(UObject* ReplicatedObj)
{
	TArray<FChanneldReplicatorBase*> Result;
	// Recurse the base class until find the matching replicator
	for (const UClass* Class = ReplicatedObj->GetClass(); Class != UObject::StaticClass(); Class = Class->GetSuperClass())
	{
		if (ReplicatorRegistry.Contains(Class))
		{
			const FReplicatorCreateFunc& Func = ReplicatorRegistry.FindRef(Class);
			auto Replicator = Func(ReplicatedObj);
			UE_LOG(LogChanneld, Verbose, TEXT("Created %sReplicator for object: %s"), *Class->GetName(), *ReplicatedObj->GetName());
			// Add the replicators in the order as base class -> inherited class (e.g. Actor->GameStateBase),
			// to make sure the property replication and OnRep functions are executed in the right order (e.g. Actor.Role -> GameStateBase.bReplicatedHasBegunPlay -> NotifyBeginPlay())
			Result.Insert(Replicator, 0);
		}
	}

	//if (Result.Num() == 0)
	//{
	//	UE_LOG(LogChanneld, Log, TEXT("Unable to find replicator for %s, registry size: %d"), *ReplicatedObj->GetFullName(), ReplicatorRegistry.Num());
	//}
	return Result;
}