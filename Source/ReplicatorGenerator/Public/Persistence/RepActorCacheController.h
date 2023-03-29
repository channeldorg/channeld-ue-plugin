// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "JsonModel.h"
#include "RepActorCacheController.generated.h"

USTRUCT(BlueprintType)
struct REPLICATORGENERATOR_API FRepActorCacheRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FString TargetClassPath;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FString ParentClassPath;

	FRepActorCacheRow() = default;

	FRepActorCacheRow(const FString& InTargetClassPath)
		: TargetClassPath(InTargetClassPath)
	{
	}

	FRepActorCacheRow(const FString& InTargetClassPath, const FString& InParentClassPath)
		: TargetClassPath(InTargetClassPath), ParentClassPath(InParentClassPath)
	{
	}
};

USTRUCT(BlueprintType)
struct REPLICATORGENERATOR_API FRepActorCache
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FDateTime CacheTime;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FRepActorCacheRow> RepActorCacheRows;

	FRepActorCache() = default;

	FRepActorCache(const TArray<FRepActorCacheRow>& InRepActorCacheRows)
		: RepActorCacheRows(InRepActorCacheRows)
	{
		CacheTime = FDateTime::UtcNow();
	}
};

struct FRepActorDependency
{
	FString TargetClassPath;

	TWeakPtr<FRepActorDependency> ParentClassPath;

	TArray<TWeakPtr<FRepActorDependency>> ChildClassPaths;

	FRepActorDependency() = default;

	FRepActorDependency(const FString& InTargetClassPath)
		: TargetClassPath(InTargetClassPath)
	{
	}
};

UCLASS(BlueprintType)
class REPLICATORGENERATOR_API URepActorCacheController : public UEngineSubsystem
{
	GENERATED_BODY()

protected:
	FDateTime LatestRepActorCacheTime;
	TArray<FRepActorCacheRow> RepActorCacheRows;
	TMap<FString, TSharedRef<FRepActorDependency>> RepActorDependencyMap;

	TJsonModel<FRepActorCache> RepActorCacheModel;

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	bool SaveRepActorCache(const TArray<const UClass*>& InRepActorClasses);
	
	UFUNCTION(BlueprintCallable)
	bool NeedToRefreshCache();

	UFUNCTION(BlueprintCallable)
	void GetRepActorClassPaths(TArray<FString>& OutRepActorClassPaths);

	UFUNCTION(BlueprintCallable)
	void GetParentClassPaths(const FString& InTargetClassPath, TArray<FString>& OutParentClassPaths);

	UFUNCTION(BlueprintCallable)
	void GetChildClassPaths(const FString& InTargetClassPath, TArray<FString>& OutChildClassPaths);

protected:
	void SetRepActorCacheRows(const TArray<FRepActorCacheRow>& InRepActorCacheRows);

	void EnsureLatestRepActorCache();
};
