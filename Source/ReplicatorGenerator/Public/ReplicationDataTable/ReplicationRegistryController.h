#pragma once
#include "ReplicationDataTable.h"
#include "ReplicationRegistryController.generated.h"

USTRUCT(BlueprintType)
struct REPLICATORGENERATOR_API FChanneldReplicationRegistryRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString TargetClassPath;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool Skip = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool Singleton = false;

	FChanneldReplicationRegistryRow() = default;

	FChanneldReplicationRegistryRow(const FString& InTargetClassPath)
		: TargetClassPath(InTargetClassPath), Skip(false), Singleton(false)
	{
	}

	virtual FName GetRowName() const
	{
		return FName(TargetClassPath);
	}
};

class REPLICATORGENERATOR_API FReplicationRegistryController

{
	FReplicationRegistryController();
	~FReplicationRegistryController() = default;
	FReplicationRegistryController(const FReplicationRegistryController&) = delete;
	FReplicationRegistryController(FReplicationRegistryController&&) = delete;
	const FReplicationRegistryController& operator=(const FReplicationRegistryController&) = delete;

public:
	static FReplicationRegistryController& Get();

	bool GetItems(TArray<FChanneldReplicationRegistryRow*>& OutItems);

	void GetItems_Unsafe(TArray<FChanneldReplicationRegistryRow*>& OutItems);

	bool UpdateRegistryTable(const TArray<FChanneldReplicationRegistryRow>& ItemsToAdd, const TArray<FString>& ItemsToRemove);

	bool PromptForSaveAndCloseRegistryTable();

private:
	TReplicationDataTable<FChanneldReplicationRegistryRow> RepRegistryModal;
};
