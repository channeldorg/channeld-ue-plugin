#include "ChanneldReplication.h"

TMap<const UClass*, const FReplicatorCreateFunc> ChanneldReplication::ReplicatorRegistry;
TMap<const FString, const FReplicatorCreateFunc> ChanneldReplication::BPReplicatorRegistry;

void ChanneldReplication::RegisterReplicator(const UClass* TargetClass, const FReplicatorCreateFunc& Func)
{
	ReplicatorRegistry.Add(TargetClass, Func);
	UE_LOG(LogChanneld, Log, TEXT("Registered replicator for %s, registry size: %d"), *TargetClass->GetFullName(), ReplicatorRegistry.Num());
}

void ChanneldReplication::RegisterReplicator(const FString& PathName, const FReplicatorCreateFunc& Func)
{
	BPReplicatorRegistry.Add(PathName, Func);
	UE_LOG(LogChanneld, Log, TEXT("Registered replicator for %s, registry size: %d"), *PathName, BPReplicatorRegistry.Num());
}

// TODO: use the pool
TArray<FChanneldReplicatorBase*> ChanneldReplication::FindAndCreateReplicators(UObject* ReplicatedObj)
{
	TArray<FChanneldReplicatorBase*> Result;
	// Recurse the base class until find the matching replicator
	for (const UClass* Class = ReplicatedObj->GetClass(); Class != UObject::StaticClass(); Class = Class->GetSuperClass())
	{
		const FReplicatorCreateFunc* Func = nullptr;
		if (ReplicatorRegistry.Contains(Class))
		{
			Func = ReplicatorRegistry.Find(Class);
		}
		else if (Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint) && BPReplicatorRegistry.Contains(Class->GetPathName()))
		{
			Func = BPReplicatorRegistry.Find(Class->GetPathName());
		}
		else
		{
			continue;
		}
		if (Func == nullptr)
		{
			UE_LOG(LogChanneld, Log, TEXT("Class %s was registered, but the replicator construction function is nullptr"), *ReplicatedObj->GetName(), BPReplicatorRegistry.Num());
			continue;
		}
		auto Replicator = (*Func)(ReplicatedObj);
		UE_LOG(LogChanneld, Verbose, TEXT("Created %sReplicator for object: %s"), *Class->GetName(), *ReplicatedObj->GetName());
		// Add the replicators in the order as base class -> inherited class (e.g. Actor->GameStateBase),
		// to make sure the property replication and OnRep functions are executed in the right order (e.g. Actor.Role -> GameStateBase.bReplicatedHasBegunPlay -> NotifyBeginPlay())
		Result.Insert(Replicator, 0);
	}

	//if (Result.Num() == 0)
	//{
	//	UE_LOG(LogChanneld, Log, TEXT("Unable to find replicator for %s, registry size: %d"), *ReplicatedObj->GetFullName(), ReplicatorRegistry.Num());
	//}
	return Result;
}