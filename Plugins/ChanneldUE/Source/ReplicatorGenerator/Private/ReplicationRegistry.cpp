#include "ReplicationRegistry.h"
#include "AssetToolsModule.h"
#include "FileHelpers.h"
#include "Engine/DataTable.h"
#include "Factories/DataTableFactory.h"

#define LOCTEXT_NAMESPACE "ReplicationRegistryManager"

FString ReplicationRegistryUtils::GetRegistryTableAssetFullName()
{
	return FString::Printf(TEXT("DataTable\'%s/%s.%s\'"), *GetRegistryTablePackagePath(), *GetRegistryTableAssetName(), *GetRegistryTableAssetName());
}

FString ReplicationRegistryUtils::GetRegistryTableAssetName()
{
	return TEXT("ReplicationRegistry");
}

FString ReplicationRegistryUtils::GetRegistryTablePackagePath()
{
	return TEXT("/Game/ChanneldUE");
}

UDataTable* ReplicationRegistryUtils::LoadRegistryTable()
{
	UDataTable* RegistryTable = TryLoadRegistryTable();
	if (RegistryTable == nullptr)
	{
		RegistryTable = CreateRegistryTable();
	}
	return RegistryTable;
}

UDataTable* ReplicationRegistryUtils::TryLoadRegistryTable()
{
	return Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, *GetRegistryTableAssetFullName()));
}

UDataTable* ReplicationRegistryUtils::CreateRegistryTable()
{
	const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	UDataTableFactory* Factory = NewObject<UDataTableFactory>();
	Factory->Struct = FChanneldReplicationRegistryItem::StaticStruct();
	UObject* NewAsset = AssetToolsModule.Get().CreateAsset(GetRegistryTableAssetName(), GetRegistryTablePackagePath(), UDataTable::StaticClass(), Factory);
	UDataTable* NewDataTableAsset = CastChecked<UDataTable>(NewAsset);
	SaveRegistryTable(NewDataTableAsset);
	return NewDataTableAsset;
}

TArray<FChanneldReplicationRegistryItem*> ReplicationRegistryUtils::GetRegistryTableData(const UDataTable* RegistryTable)
{
	TArray<FChanneldReplicationRegistryItem*> Result;
	RegistryTable->GetAllRows<FChanneldReplicationRegistryItem>(TEXT("FChanneldReplicationRegistryItem"), Result);
	return Result;
}

void ReplicationRegistryUtils::AddItemsToRegistryTable(UDataTable* RegistryTable, const TArray<FString>& TargetClassPaths)
{
	for (const FString& TargetClassPath : TargetClassPaths)
	{
		AddItemToRegistryTable(RegistryTable, TargetClassPath);
	}
}

void ReplicationRegistryUtils::AddItemToRegistryTable(UDataTable* RegistryTable, const FString& TargetClassPath)
{
	RegistryTable->AddRow(FName(TargetClassPath), FChanneldReplicationRegistryItem(TargetClassPath));
}

void ReplicationRegistryUtils::RemoveItemsFromRegistryTable(UDataTable* RegistryTable, const TArray<FString>& TargetClassPaths)
{
	for (const FString& TargetClassPath : TargetClassPaths)
	{
		RemoveItemFromRegistryTable(RegistryTable, TargetClassPath);
	}
}

void ReplicationRegistryUtils::RemoveItemFromRegistryTable(UDataTable* RegistryTable, const FString& TargetClassPath)
{
	RegistryTable->RemoveRow(FName(TargetClassPath));
}

bool ReplicationRegistryUtils::SaveRegistryPackage(UPackage* RegistryPackage)
{
	return UEditorLoadingAndSavingUtils::SavePackages(TArray<UPackage*>{RegistryPackage}, false);
}

bool ReplicationRegistryUtils::SaveRegistryTable(const UDataTable* RegistryTable)
{
	return SaveRegistryPackage(RegistryTable->GetOutermost());
}

bool ReplicationRegistryUtils::PromptForSaveRegistryTable(const UDataTable* RegistryTable)
{
	UPackage* RegistryPackage = RegistryTable->GetOutermost();
	if (RegistryPackage->IsDirty())
	{
		const FText DialogText = LOCTEXT("SaveRegistryTableDialogText", "The ReplicationRegistry has been modified. Do you want to save it?");
		const EAppReturnType::Type ReturnType = FMessageDialog::Open(EAppMsgType::OkCancel, DialogText);
		if (ReturnType == EAppReturnType::Type::Ok)
		{
			return SaveRegistryPackage(RegistryPackage);
		}
		else
		{
			return false;
		}
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
