#include "ChanneldReplication.h"

TMap<const UClass*, const FReplicatorCreateFunc> ChanneldReplication::ReplicatorRegistry;
TMap<const FString, const FReplicatorCreateFunc> ChanneldReplication::BPReplicatorRegistry;
TArray<FReplicatorStateInProto> ChanneldReplication::ReplicatorStatesInProto;
TMap<const UClass*, FReplicatorStateInProto> ChanneldReplication::ReplicatorTargetClassToStateInProto;
TMap<const FName, IChannelDataProcessor*> ChanneldReplication::ChannelDataProcessorRegistry;

FReplicatorStateInProto* ChanneldReplication::FindReplicatorStateInProto(const UClass* TargetClass)
{
	// Find in the cached map
	FReplicatorStateInProto* Result = ReplicatorTargetClassToStateInProto.Find(TargetClass);
	if (Result)
	{
		return Result;
	}

	// Find in the original array
	for (FReplicatorStateInProto& State : ReplicatorStatesInProto)
	{
		if (State.TargetClass)
		{
			if (State.TargetClass == TargetClass)
			{
				Result = &State;
				break;
			}
		}
		else
		{
			if (State.TargetClassPathName == TargetClass->GetPathName())
			{
				State.TargetClass = TargetClass;
				Result = &State;
				break;;
			}
		}
	}

	if (Result)
	{
		// Update the cached map
		ReplicatorTargetClassToStateInProto.Add(TargetClass, *Result);
	}
	
	return Result;
}

void ChanneldReplication::RegisterReplicator(const UClass* TargetClass, const FReplicatorCreateFunc& Func, bool bOverride, bool bIsInMap)
{
	bool bExists = ReplicatorRegistry.Contains(TargetClass);
	if (!bOverride && bExists)
	{
		UE_LOG(LogChanneld, Log, TEXT("%s already exists in the replicator registry, will not be added."), *TargetClass->GetFullName());
		return;
	}
	ReplicatorRegistry.Add(TargetClass, Func);
	ReplicatorRegistry.Emplace(TargetClass, Func);
	UE_LOG(LogChanneld, Log, TEXT("Registered replicator for %s, registry size: %d"), *TargetClass->GetFullName(), ReplicatorRegistry.Num());

	if (!bExists)
	{
		ReplicatorStatesInProto.Add(FReplicatorStateInProto(ReplicatorStatesInProto.Num(), TargetClass, nullptr, bIsInMap));
	}
}

void ChanneldReplication::RegisterReplicator(const FString& PathName, const FReplicatorCreateFunc& Func, bool bOverride, bool bIsInMap)
{
	bool bExists = BPReplicatorRegistry.Contains(PathName);
	if (!bOverride && bExists)
	{
		UE_LOG(LogChanneld, Log, TEXT("%s already exists in the replicator registry, will not be added."), *PathName);
		return;
	}
	BPReplicatorRegistry.Add(PathName, Func);
	UE_LOG(LogChanneld, Log, TEXT("Registered replicator for %s, registry size: %d"), *PathName, BPReplicatorRegistry.Num());

	if (!bExists)
	{
		ReplicatorStatesInProto.Add(FReplicatorStateInProto(ReplicatorStatesInProto.Num(), nullptr, &PathName, bIsInMap));
	}
}

// TODO: use the pool
TArray<FChanneldReplicatorBase*> ChanneldReplication::FindAndCreateReplicators(UObject* ReplicatedObj, const UClass* SkipRootClass /*= nullptr*/)
{
	TArray<FChanneldReplicatorBase*> Result;
	// Recurse the base class until find the matching replicator
	for (const UClass* Class = ReplicatedObj->GetClass(); Class != SkipRootClass; Class = Class->GetSuperClass())
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
		// to make sure the property updates are executed in the right order (e.g. Actor.Role -> GameStateBase.bReplicatedHasBegunPlay)
		Result.Insert(Replicator, 0);
	}

	//if (Result.Num() == 0)
	//{
	//	UE_LOG(LogChanneld, Log, TEXT("Unable to find replicator for %s, registry size: %d"), *ReplicatedObj->GetFullName(), ReplicatorRegistry.Num());
	//}
	return Result;
}

void ChanneldReplication::RegisterChannelDataProcessor(const FName& MessageFullName, IChannelDataProcessor* Processor)
{
	ChannelDataProcessorRegistry.Add(MessageFullName, Processor);
	UE_LOG(LogChanneld, Log, TEXT("Registered processor for %s"), *MessageFullName.ToString());
}

uint32 ChanneldReplication::GetUnrealObjectType(const UObject* Obj)
{
	if (Obj == nullptr)
	{
		return 0;
	}
	
	const UClass* ObjClass = Obj->GetClass();
	// Find in the descending order, so the most derived class will be found first
	for (int i = ReplicatorStatesInProto.Num() - 1; i >= 0; i--)
	{
		FReplicatorStateInProto& State = ReplicatorStatesInProto[i];
		
		if (State.TargetClass)
		{
			for (const UClass* Class = ObjClass; Class != UObject::StaticClass(); Class = Class->GetSuperClass())
			{
				if (Class == State.TargetClass)
				{
					return State.FieldIndex + 1;
				}
			}
		}
		else
		{
			for (const UClass* Class = ObjClass; Class != UObject::StaticClass(); Class = Class->GetSuperClass())
			{
				if (Class->GetPathName() == State.TargetClassPathName)
				{
					return State.FieldIndex + 1;
				}
			}
		}
	}

	return 0;
}
