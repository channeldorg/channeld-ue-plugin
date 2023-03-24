#pragma once

#include "Engine/DataTable.h"

template <typename InDataTableRowType>
class REPLICATORGENERATOR_API TReplicationDataTable : public FGCObject
{
public:
	TReplicationDataTable() = default;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	virtual bool IsOccupied() const;

	virtual bool StartOccupancy();

	virtual bool StopOccupancy();

	virtual FString GetDataTableAssetFullName();

	virtual FString GetDataTableOuterPackagePath();

	virtual FString GetDataTableAssetName();
	virtual void SetDataTableAssetName(const FString& InDataTableAssetName);

	virtual bool GetItems(TArray<InDataTableRowType*>& OutDataTableRows);

	virtual void GetItems_Unsafe(TArray<InDataTableRowType*>& OutDataTableRows);

	virtual bool AddItems(const TArray<InDataTableRowType>& DataTableRows);

	virtual bool AddItem(const InDataTableRowType& DataTableRow);

	virtual bool RemoveItems(const TArray<FName>& RowNames);

	virtual bool RemoveItem(const FName& RowName);

	virtual bool SetAll(const TArray<InDataTableRowType>& DataTableRows);

	virtual bool SaveDataTable();

protected:
	FString DataTableAssetName;

	UDataTable* DataTable;

	bool bOccupied;
	FCriticalSection OccupiedMutex;

	virtual bool EnsureDataTableAvailable();

	virtual void LoadDataTable();

	virtual void TryLoadDataTable();

	virtual void CreateDataTable();

	virtual bool SaveDataTablePackage(UPackage* DataTablePackage);
};
