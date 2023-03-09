#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ReplicationRegistry.generated.h"

USTRUCT(BlueprintType)
struct REPLICATORGENERATOR_API FChanneldReplicationRegistryItem : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString TargetClassPath;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool Skip = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool Singleton = false;

	FChanneldReplicationRegistryItem() = default;

	FChanneldReplicationRegistryItem(const FString& InTargetClassPath)
		: TargetClassPath(InTargetClassPath), Skip(false), Singleton(false)
	{
	}
};

namespace ReplicationRegistryUtils
{
	REPLICATORGENERATOR_API inline FString GetRegistryTableAssetFullName();
	
	REPLICATORGENERATOR_API inline FString GetRegistryTableAssetName();

	REPLICATORGENERATOR_API inline FString GetRegistryTablePackagePath();

	REPLICATORGENERATOR_API inline UDataTable* LoadRegistryTable();
	REPLICATORGENERATOR_API inline UDataTable* TryLoadRegistryTable();

	REPLICATORGENERATOR_API inline UDataTable* CreateRegistryTable();

	REPLICATORGENERATOR_API inline TArray<FChanneldReplicationRegistryItem*> GetRegistryTableData(const UDataTable* RegistryTable);

	REPLICATORGENERATOR_API inline void AddItemsToRegistryTable(UDataTable* RegistryTable, const TArray<FString>& TargetClassPaths);

	REPLICATORGENERATOR_API inline void AddItemToRegistryTable(UDataTable* RegistryTable, const FString& TargetClassPath);

	REPLICATORGENERATOR_API inline void RemoveItemsFromRegistryTable(UDataTable* RegistryTable, const TArray<FString>& TargetClassPaths);

	REPLICATORGENERATOR_API inline void RemoveItemFromRegistryTable(UDataTable* RegistryTable, const FString& TargetClassPath);

	REPLICATORGENERATOR_API inline bool SaveRegistryPackage(UPackage* RegistryPackage);

	REPLICATORGENERATOR_API inline bool SaveRegistryTable(const UDataTable* RegistryTable);

	REPLICATORGENERATOR_API inline bool PromptForSaveRegistryTable(const UDataTable* RegistryTable);
};
