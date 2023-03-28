// Fill out your copyright notice in the Description page of Project Settings.


#include "Persistence/ReplicationDataTable.h"
#include "AssetToolsModule.h"
#include "FileHelpers.h"
#include "Factories/DataTableFactory.h"

template <typename InDataTableRowType>
void TReplicationDataTable<InDataTableRowType>::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (DataTable != nullptr)
		Collector.AddReferencedObject(DataTable);
}

template <typename InDataTableRowType>
bool TReplicationDataTable<InDataTableRowType>::IsOccupied() const
{
	return bOccupied;
}

template <typename InDataTableRowType>
bool TReplicationDataTable<InDataTableRowType>::StartOccupancy()
{
	FScopeLock ScopeLock(&OccupiedMutex);
	if (IsOccupied())
	{
		return false;
	}
	// TODO: Check if the DataTable is occupied by editor.
	DataTable = nullptr;
	CollectGarbage(RF_NoFlags);
	bOccupied = true;
	return true;
}

template <typename InDataTableRowType>
bool TReplicationDataTable<InDataTableRowType>::StopOccupancy()
{
	bOccupied = false;
	return true;
}

template <typename InDataTableRowType>
bool TReplicationDataTable<InDataTableRowType>::IsExist()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	const FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*FString::Printf(TEXT("%s/%s.%s"), *GetDataTableOuterPackagePath(), *GetDataTableAssetName(), *GetDataTableAssetName()));
	return AssetData.IsValid();
}

template <typename InDataTableRowType>
FString TReplicationDataTable<InDataTableRowType>::GetDataTableAssetFullName()
{
	return FString::Printf(TEXT("DataTable\'%s/%s.%s\'"), *GetDataTableOuterPackagePath(), *GetDataTableAssetName(), *GetDataTableAssetName());
}

template <typename InDataTableRowType>
FString TReplicationDataTable<InDataTableRowType>::GetDataTableOuterPackagePath()
{
	return TEXT("/Game/ChanneldUE");
}

template <typename InDataTableRowType>
FString TReplicationDataTable<InDataTableRowType>::GetDataTableAssetName()
{
	return DataTableAssetName;
}

template <typename InDataTableRowType>
void TReplicationDataTable<InDataTableRowType>::SetDataTableAssetName(const FString& InDataTableAssetName)
{
	DataTableAssetName = InDataTableAssetName;
}

template <typename InDataTableRowType>
bool TReplicationDataTable<InDataTableRowType>::GetItems(TArray<InDataTableRowType*>& OutDataTableRows)
{
	FScopeLock ScopeLock(&OccupiedMutex);
	if (!EnsureDataTableAvailable())
	{
		return false;
	}
	DataTable->GetAllRows<InDataTableRowType>(*InDataTableRowType::StaticStruct()->GetName(), OutDataTableRows);
	return true;
}

template <typename InDataTableRowType>
void TReplicationDataTable<InDataTableRowType>::GetItems_Unsafe(TArray<InDataTableRowType*>& OutDataTableRows)
{
	LoadDataTable();
	DataTable->GetAllRows<InDataTableRowType>(*InDataTableRowType::StaticStruct()->GetName(), OutDataTableRows);
}

template <typename InDataTableRowType>
bool TReplicationDataTable<InDataTableRowType>::AddItems(const TArray<InDataTableRowType>& DataTableRows)
{
	FScopeLock ScopeLock(&OccupiedMutex);
	if (!EnsureDataTableAvailable())
	{
		return false;
	}
	for (const InDataTableRowType& DataTableRow : DataTableRows)
	{
		DataTable->AddRow(DataTableRow.GetRowName(), DataTableRow);
	}
	return true;
}

template <typename InDataTableRowType>
bool TReplicationDataTable<InDataTableRowType>::AddItem(const InDataTableRowType& DataTableRow)
{
	FScopeLock ScopeLock(&OccupiedMutex);
	if (!EnsureDataTableAvailable())
	{
		return false;
	}
	DataTable->AddRow(DataTableRow.GetRowName(), DataTableRow);
	return true;
}

template <typename InDataTableRowType>
bool TReplicationDataTable<InDataTableRowType>::RemoveItems(const TArray<FName>& RowNames)
{
	FScopeLock ScopeLock(&OccupiedMutex);
	if (!EnsureDataTableAvailable())
	{
		return false;
	}
	for (const FName& RowName : RowNames)
	{
		DataTable->RemoveRow(RowName);
	}
	return true;
}

template <typename InDataTableRowType>
bool TReplicationDataTable<InDataTableRowType>::RemoveItem(const FName& RowName)
{
	FScopeLock ScopeLock(&OccupiedMutex);
	if (!EnsureDataTableAvailable())
	{
		return false;
	}
	DataTable->RemoveRow(RowName);
	return true;
}

template <typename InDataTableRowType>
bool TReplicationDataTable<InDataTableRowType>::SetAll(const TArray<InDataTableRowType>& DataTableRows)
{
	FScopeLock ScopeLock(&OccupiedMutex);
	if (!EnsureDataTableAvailable())
	{
		return false;
	}
	DataTable->EmptyTable();
	for (const InDataTableRowType& DataTableRow : DataTableRows)
	{
		DataTable->AddRow(DataTableRow.GetRowName(), DataTableRow);
	}
	return true;
}

template <typename InDataTableRowType>
bool TReplicationDataTable<InDataTableRowType>::SaveDataTablePackage(UPackage* DataTablePackage)
{
	FScopeLock ScopeLock(&OccupiedMutex);
	if (DataTablePackage == nullptr)
	{
		return false;
	}
	if (IsOccupied())
	{
		return false;
	}
	return UEditorLoadingAndSavingUtils::SavePackages(TArray<UPackage*>{DataTablePackage}, false);
}

template <typename InDataTableRowType>
bool TReplicationDataTable<InDataTableRowType>::SaveDataTable()
{
	FScopeLock ScopeLock(&OccupiedMutex);
	if (DataTable == nullptr)
	{
		return false;
	}
	if (IsOccupied())
	{
		return false;
	}
	return UEditorLoadingAndSavingUtils::SavePackages(TArray<UPackage*>{DataTable->GetOutermost()}, false);
}

template <typename InDataTableRowType>
bool TReplicationDataTable<InDataTableRowType>::EnsureDataTableAvailable()
{
	if (IsOccupied())
	{
		return false;
	}
	LoadDataTable();
	return true;
}

template <typename InDataTableRowType>
void TReplicationDataTable<InDataTableRowType>::LoadDataTable()
{
	if (DataTable == nullptr)
		TryLoadDataTable();
	if (DataTable == nullptr)
		CreateDataTable();
}

template <typename InDataTableRowType>
void TReplicationDataTable<InDataTableRowType>::TryLoadDataTable()
{
	DataTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, *GetDataTableAssetFullName()));
}

template <typename InDataTableRowType>
void TReplicationDataTable<InDataTableRowType>::CreateDataTable()
{
	const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	UDataTableFactory* Factory = NewObject<UDataTableFactory>();
	Factory->Struct = InDataTableRowType::StaticStruct();
	UObject* NewAsset = AssetToolsModule.Get().CreateAsset(GetDataTableAssetName(), GetDataTableOuterPackagePath(), UDataTable::StaticClass(), Factory);
	DataTable = CastChecked<UDataTable>(NewAsset);
	SaveDataTable();
}
