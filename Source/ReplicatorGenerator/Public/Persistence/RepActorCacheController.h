// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "JsonModel.h"
#include "ReplicatorGeneratorDefinition.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/WorldSettings.h"
#include "RepActorCacheController.generated.h"

USTRUCT(BlueprintType)
struct REPLICATORGENERATOR_API FRepActorRelationCache
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FString TargetClassPath;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bIsComponent = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bIsChildOfGameState = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bIsChildOfWorldSetting = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FString ParentClassPath;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FString> ComponentClassPaths;

	FRepActorRelationCache() = default;

	FRepActorRelationCache(const UClass* InTargetClass)
		: TargetClassPath(InTargetClass->GetPathName())
	{
		if (InTargetClass->IsChildOf(UActorComponent::StaticClass()))
		{
			bIsComponent = true;
		}
		if (InTargetClass->IsChildOf(AGameStateBase::StaticClass()))
		{
			bIsChildOfGameState = true;
		}
		if(InTargetClass->IsChildOf(AWorldSettings::StaticClass()))
		{
			bIsChildOfWorldSetting = true;
		}
	}
};

USTRUCT(BlueprintType)
struct REPLICATORGENERATOR_API FRepActorCache
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FDateTime CacheTime;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FRepActorRelationCache> RepActorRelationCaches;

	FRepActorCache() = default;

	FRepActorCache(const TArray<FRepActorRelationCache>& InRepActorRelationCaches)
		: RepActorRelationCaches(InRepActorRelationCaches)
	{
		CacheTime = FDateTime::UtcNow();
	}
};

struct FRepActorDependency
{
	FRepActorRelationCache RelationCache;

	FString TargetClassPath;

	TWeakPtr<FRepActorDependency> Parent;

	TArray<FString> SuperClassPaths;

	TArray<TWeakPtr<FRepActorDependency>> Children;

	TArray<TWeakPtr<FRepActorDependency>> Components;

	TArray<FString> ComponentUserClassPaths;

	FRepActorDependency() = default;

	FRepActorDependency(const FRepActorRelationCache& InRelationCache)
		: RelationCache(InRelationCache), TargetClassPath(InRelationCache.TargetClassPath)
	{
	}
};

UCLASS(BlueprintType)
class REPLICATORGENERATOR_API URepActorCacheController : public UEditorSubsystem
{
	GENERATED_BODY()

protected:
	FDateTime LatestRepActorCacheTime;
	TArray<FRepActorRelationCache> RepActorRelationCaches;
	TMap<FString, TSharedRef<FRepActorDependency>> RepActorDependencyMap;

	TJsonModel<FRepActorCache> RepActorCacheModel = GenManager_RepActorCachePath;

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	bool SaveRepActorCache(const TArray<const UClass*>& InRepActorClasses);

	UFUNCTION(BlueprintCallable)
	bool NeedToRefreshCache();

	UFUNCTION(BlueprintCallable, BlueprintPure)
	bool IsDefaultSingleton(const FString& InTargetClassPath);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	void GetRepActorClassPaths(TArray<FString>& OutRepActorClassPaths);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	void GetParentClassPaths(const FString& InTargetClassPath, TArray<FString>& OutParentClassPaths);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	void GetChildClassPaths(const FString& InTargetClassPath, TArray<FString>& OutChildClassPaths);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	void GetComponentClassPaths(const FString& InTargetClassPath, TArray<FString>& OutComponentClassPaths);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	void GetComponentUserClassPaths(const FString& InTargetClassPath, TArray<FString>& OutUserClassPaths);

protected:
	void SetRepActorRelationCaches(const TArray<FRepActorRelationCache>& InRepActorRelationCaches);

	void EnsureLatestRepActorCache();
};
