#include "ReplicatorGeneratorDefinition.h"
#include "Persistence/RepActorCacheController.h"

void URepActorCacheController::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	RepActorCacheModal.SetDataFilePath(GenManager_IntermediateDir / TEXT("ReplicationActorCache.json"));
}

bool URepActorCacheController::SaveRepActorCache(const TArray<const UClass*>& InRepActorClasses)
{
	TSet<const UClass*> RepActorClassesSet;
	for (const UClass* RepActorClass : InRepActorClasses)
	{
		RepActorClassesSet.Add(RepActorClass);
	}
	TArray<const UClass*> SortedRepActorClassArr = RepActorClassesSet.Array();
	SortedRepActorClassArr.Sort([](const UClass& Lhs, const UClass& Rhs)
	{
		return Lhs.GetPathName() < Rhs.GetPathName();
	});
	TArray<FRepActorCacheRow> NewRepActorCacheRows;
	for (const UClass* RepActorClass : SortedRepActorClassArr)
	{
		if(RepActorClass == AActor::StaticClass() || RepActorClass == UActorComponent::StaticClass())
		{
			NewRepActorCacheRows.Add(FRepActorCacheRow(RepActorClass->GetPathName()));
			continue;
		}
		const UClass* ParentClass = RepActorClass->GetSuperClass();
		while (ParentClass)
		{
			// Maybe the parent class is not a replication actor class, so we need to skip it, and find the parent class of the parent class.
			if (RepActorClassesSet.Contains(ParentClass) || ParentClass == AActor::StaticClass() || ParentClass == UActorComponent::StaticClass())
			{
				NewRepActorCacheRows.Add(FRepActorCacheRow(RepActorClass->GetPathName(), ParentClass->GetPathName()));
				break;
			}
			ParentClass = ParentClass->GetSuperClass();
		}
	}
	return RepActorCacheModal.SaveData(NewRepActorCacheRows);
}

void URepActorCacheController::LoadRepActorCache()
{
	TArray<FRepActorCacheRow> NewRepActorCacheRows;
	RepActorCacheModal.LoadData(NewRepActorCacheRows);
	SetRepActorCacheRows(NewRepActorCacheRows);
}

void URepActorCacheController::GetRepActorClassPaths(TArray<FString>& OutRepActorClassPaths)
{
	OutRepActorClassPaths.Empty(RepActorCacheRows.Num());
	for (const FRepActorCacheRow& RepActorCacheRow : RepActorCacheRows)
	{
		OutRepActorClassPaths.Add(RepActorCacheRow.TargetClassPath);
	}
}

void URepActorCacheController::GetParentClassPaths(const FString& InTargetClassPath, TArray<FString>& OutParentClassPaths)
{
	if (!RepActorDependencyMap.Contains(InTargetClassPath))
	{
		return;
	}
	OutParentClassPaths.Empty();
	TWeakPtr<FRepActorDependency> RepActorDependency = RepActorDependencyMap.FindRef(InTargetClassPath);
	if(RepActorDependency.IsValid())
	{
		RepActorDependency = RepActorDependency.Pin()->ParentClassPath;
	}
	while (RepActorDependency.IsValid())
	{
		OutParentClassPaths.Add(RepActorDependency.Pin()->TargetClassPath);
		RepActorDependency = RepActorDependency.Pin()->ParentClassPath;
	}
}

void URepActorCacheController::GetChildClassPaths(const FString& InTargetClassPath, TArray<FString>& OutChildClassPaths)
{
	if (!RepActorDependencyMap.Contains(InTargetClassPath))
	{
		return;
	}
	const TWeakPtr<FRepActorDependency> RepActorDependency = RepActorDependencyMap.FindRef(InTargetClassPath);
	if (!RepActorDependency.IsValid()) return;
	OutChildClassPaths.Empty();
	for (const TWeakPtr<FRepActorDependency>& ChildRepActorDependency : RepActorDependency.Pin()->ChildClassPaths)
	{
		OutChildClassPaths.Add(ChildRepActorDependency.Pin()->TargetClassPath);
	}
}

void URepActorCacheController::SetRepActorCacheRows(const TArray<FRepActorCacheRow>& InRepActorCacheRows)
{
	RepActorCacheRows = InRepActorCacheRows;
	RepActorDependencyMap.Empty(RepActorCacheRows.Num());
	for (FRepActorCacheRow& RepActorCacheRow : RepActorCacheRows)
	{
		RepActorDependencyMap.Add(RepActorCacheRow.TargetClassPath, MakeShared<FRepActorDependency>(RepActorCacheRow.TargetClassPath));
	}
	for (FRepActorCacheRow& RepActorCacheRow : RepActorCacheRows)
	{
		if (!RepActorDependencyMap.Contains(RepActorCacheRow.ParentClassPath))
		{
			continue;
		}
		const TSharedRef<FRepActorDependency>& RepActorDependency = RepActorDependencyMap.FindRef(RepActorCacheRow.TargetClassPath);
		const TSharedRef<FRepActorDependency>& ParentRepActorDependency = RepActorDependencyMap.FindRef(RepActorCacheRow.ParentClassPath);
		RepActorDependency->ParentClassPath = ParentRepActorDependency;
		ParentRepActorDependency->ChildClassPaths.Add(RepActorDependency);
	}
}
