#include "ReplicatorGeneratorUtils.h"
#include "Persistence/RepActorCacheController.h"

void URepActorCacheController::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

FRepActorCache URepActorCacheController::ConvertClassesToRepActorCache(const TArray<const UClass*>& InRepActorClasses)
{
	TMap<const UClass*, FRepActorRelationCache> RepActorClassesMap;
	RepActorClassesMap.Empty(InRepActorClasses.Num());
	for (const UClass* RepActorClass : InRepActorClasses)
	{
		RepActorClassesMap.Add(RepActorClass, FRepActorRelationCache(RepActorClass));
	}
	for (const UClass* RepActorClass : InRepActorClasses)
	{
		for (const UClass* CompClass : ChanneldReplicatorGeneratorUtils::GetComponentClasses(RepActorClass))
		{
			if (RepActorClassesMap.Contains(CompClass))
			{
				RepActorClassesMap[RepActorClass].ComponentClassPaths.Add(RepActorClassesMap[CompClass].TargetClassPath);
			}
		}
		RepActorClassesMap[RepActorClass].ComponentClassPaths.Sort();
	}
	for (const UClass* RepActorClass : InRepActorClasses)
	{
		if (RepActorClass == AActor::StaticClass() || RepActorClass == UActorComponent::StaticClass())
		{
			continue;
		}
		const UClass* ParentClass = RepActorClass->GetSuperClass();
		while (ParentClass)
		{
			// Maybe the parent class is not a replication actor class, so we need to skip it, and find the parent class of the parent class.
			if (RepActorClassesMap.Contains(ParentClass) || ParentClass == AActor::StaticClass() || ParentClass == UActorComponent::StaticClass())
			{
				RepActorClassesMap[RepActorClass].ParentClassPath = RepActorClassesMap[ParentClass].TargetClassPath;
				break;
			}
			ParentClass = ParentClass->GetSuperClass();
		}
	}
	TArray<FRepActorRelationCache> NewRepActorRelationCaches;
	RepActorClassesMap.GenerateValueArray(NewRepActorRelationCaches);
	NewRepActorRelationCaches.Sort([](const FRepActorRelationCache& Lhs, const FRepActorRelationCache& Rhs)
	{
		return Lhs.TargetClassPath < Rhs.TargetClassPath;
	});
	return FRepActorCache(NewRepActorRelationCaches);
}

bool URepActorCacheController::SaveRepActorCache(const TArray<const UClass*>& InRepActorClasses)
{
	ChanneldReplicatorGeneratorUtils::EnsureRepGenIntermediateDir();
	return RepActorCacheModel.SaveData(ConvertClassesToRepActorCache(InRepActorClasses));
}

bool URepActorCacheController::NeedToRefreshCache()
{
	EnsureLatestRepActorCache();
	TArray<FString> Dirs = {FPaths::ProjectContentDir()};
	while (Dirs.Num() > 0)
	{
		FString Dir = Dirs.Pop(false);
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.uasset")), true, false);
		for (FString File : Files)
		{
			FDateTime FileTime = IFileManager::Get().GetTimeStamp(*(Dir / File));
			if (FileTime > LatestRepActorCacheTime)
			{
				UE_LOG(LogChanneldRepGenerator, Verbose, TEXT("File: %s, FileTime: %s, LatestRepActorCacheTime: %s"), *File, *FileTime.ToString(), *LatestRepActorCacheTime.ToString());
				return true;
			}
		}

		TArray<FString> DirsInDir;
		IFileManager::Get().FindFiles(DirsInDir, *(Dir / TEXT("*")), false, true);
		for (FString DirInDir : DirsInDir)
		{
			Dirs.Add(Dir / DirInDir);
		}
	}
	return false;
}

bool URepActorCacheController::IsDefaultSingleton(const FString& InTargetClassPath)
{
	EnsureLatestRepActorCache();
	if (!RepActorDependencyMap.Contains(InTargetClassPath))
	{
		return false;
	}
	return (
		RepActorDependencyMap[InTargetClassPath]->RelationCache.bIsChildOfGameState
		|| RepActorDependencyMap[InTargetClassPath]->RelationCache.bIsChildOfWorldSetting
	);
}

void URepActorCacheController::GetRepActorClassPaths(TArray<FString>& OutRepActorClassPaths)
{
	EnsureLatestRepActorCache();
	OutRepActorClassPaths.Empty(RepActorRelationCaches.Num());
	for (const FRepActorRelationCache& RepActorRelationCache : RepActorRelationCaches)
	{
		OutRepActorClassPaths.Add(RepActorRelationCache.TargetClassPath);
	}
}

void URepActorCacheController::GetParentClassPaths(const FString& InTargetClassPath, TArray<FString>& OutParentClassPaths)
{
	EnsureLatestRepActorCache();
	if (!RepActorDependencyMap.Contains(InTargetClassPath))
	{
		return;
	}
	OutParentClassPaths = RepActorDependencyMap[InTargetClassPath]->SuperClassPaths;
}

void URepActorCacheController::GetChildClassPaths(const FString& InTargetClassPath, TArray<FString>& OutChildClassPaths)
{
	EnsureLatestRepActorCache();
	if (!RepActorDependencyMap.Contains(InTargetClassPath))
	{
		return;
	}
	const TWeakPtr<FRepActorDependency> RepActorDependency = RepActorDependencyMap.FindRef(InTargetClassPath);
	if (!RepActorDependency.IsValid()) return;
	OutChildClassPaths.Empty(RepActorDependency.Pin()->Children.Num());
	for (const TWeakPtr<FRepActorDependency>& ChildRepActorDependency : RepActorDependency.Pin()->Children)
	{
		OutChildClassPaths.Add(ChildRepActorDependency.Pin()->TargetClassPath);
	}
}

void URepActorCacheController::GetComponentClassPaths(const FString& InTargetClassPath, TArray<FString>& OutComponentClassPaths)
{
	EnsureLatestRepActorCache();
	if (!RepActorDependencyMap.Contains(InTargetClassPath))
	{
		return;
	}
	TWeakPtr<FRepActorDependency> RepActorDependency = RepActorDependencyMap.FindRef(InTargetClassPath);
	OutComponentClassPaths.Empty();
	TSet<FString> ComponentClassPathSet;
	while (RepActorDependency.IsValid() && !RepActorDependency.Pin()->RelationCache.bIsComponent)
	{
		for (const TWeakPtr<FRepActorDependency> CompDependency : RepActorDependency.Pin()->Components)
		{
			if (!CompDependency.IsValid())
			{
				continue;
			}
			const FString& CompClassPath = CompDependency.Pin()->TargetClassPath;
			if (!ComponentClassPathSet.Contains(CompClassPath))
			{
				ComponentClassPathSet.Add(CompClassPath);
				OutComponentClassPaths.Add(CompClassPath);
			}
			for (const FString& CompParentClassPath : CompDependency.Pin()->SuperClassPaths)
			{
				if (!ComponentClassPathSet.Contains(CompParentClassPath))
				{
					ComponentClassPathSet.Add(CompParentClassPath);
					OutComponentClassPaths.Add(CompParentClassPath);
				}
			}
		}
		RepActorDependency = RepActorDependency.Pin()->Parent;
	}
}

void URepActorCacheController::GetComponentUserClassPaths(const FString& InTargetClassPath, TArray<FString>& OutUserClassPaths)
{
	EnsureLatestRepActorCache();
	if (!RepActorDependencyMap.Contains(InTargetClassPath) || !RepActorDependencyMap[InTargetClassPath]->RelationCache.bIsComponent)
	{
		return;
	}
	OutUserClassPaths = RepActorDependencyMap[InTargetClassPath]->ComponentUserClassPaths;
}

void URepActorCacheController::SetRepActorRelationCaches(const TArray<FRepActorRelationCache>& InRepActorRelationCaches)
{
	RepActorRelationCaches = InRepActorRelationCaches;
	RepActorDependencyMap.Empty(RepActorRelationCaches.Num());
	for (FRepActorRelationCache& RepActorRelationCache : RepActorRelationCaches)
	{
		RepActorDependencyMap.Add(RepActorRelationCache.TargetClassPath, MakeShared<FRepActorDependency>(RepActorRelationCache));
	}
	for (FRepActorRelationCache& RepActorRelationCache : RepActorRelationCaches)
	{
		const TSharedRef<FRepActorDependency> RepActorDependency = RepActorDependencyMap[RepActorRelationCache.TargetClassPath];
		RepActorDependency->Components.Empty(RepActorRelationCache.ComponentClassPaths.Num());
		for (const FString& CompClassPath : RepActorRelationCache.ComponentClassPaths)
		{
			RepActorDependency->Components.Add(RepActorDependencyMap[CompClassPath]);
		}

		if (!RepActorDependencyMap.Contains(RepActorRelationCache.ParentClassPath))
		{
			continue;
		}
		const TSharedRef<FRepActorDependency> ParentRepActorDependency = RepActorDependencyMap[RepActorRelationCache.ParentClassPath];
		RepActorDependency->Parent = ParentRepActorDependency;
		ParentRepActorDependency->Children.Add(RepActorDependency);
	}
	for (FRepActorRelationCache& RepActorRelationCache : RepActorRelationCaches)
	{
		const TSharedRef<FRepActorDependency> RepActorDependency = RepActorDependencyMap[RepActorRelationCache.TargetClassPath];
		TWeakPtr<FRepActorDependency> ParentRepActorDependency = RepActorDependency->Parent;
		while (ParentRepActorDependency.IsValid())
		{
			RepActorDependency->SuperClassPaths.Add(ParentRepActorDependency.Pin()->TargetClassPath);
			ParentRepActorDependency = ParentRepActorDependency.Pin()->Parent;
		}
	}
	for (FRepActorRelationCache& RepActorRelationCache : RepActorRelationCaches)
	{
		if (RepActorRelationCache.bIsComponent)
		{
			continue;
		}
		const TSharedRef<FRepActorDependency> RepActorDependency = RepActorDependencyMap[RepActorRelationCache.TargetClassPath];
		for (const TWeakPtr<FRepActorDependency>& CompDependency : RepActorDependency->Components)
		{
			CompDependency.Pin()->ComponentUserClassPaths.Add(RepActorDependency->TargetClassPath);
		}
	}
}

void URepActorCacheController::EnsureLatestRepActorCache()
{
	if (RepActorCacheModel.IsNewer())
	{
		FRepActorCache NewRepActorCache;
		RepActorCacheModel.GetData(NewRepActorCache);
		LatestRepActorCacheTime = NewRepActorCache.CacheTime;
		SetRepActorRelationCaches(NewRepActorCache.RepActorRelationCaches);
	}
}
