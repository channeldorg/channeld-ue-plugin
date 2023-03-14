#include "ReplicationRegistry.h"
#include "AssetToolsModule.h"
#include "FileHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
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

void ReplicationRegistryUtils::AddItemsToRegistryTable(UDataTable* RegistryTable, const TArray<FChanneldReplicationRegistryItem>& RegistryItems)
{
	for (const FChanneldReplicationRegistryItem& RegistryItem : RegistryItems)
	{
		AddItemToRegistryTable(RegistryTable, RegistryItem);
	}
}

void ReplicationRegistryUtils::AddItemToRegistryTable(UDataTable* RegistryTable, const FChanneldReplicationRegistryItem& RegistryItem)
{
	RegistryTable->AddRow(FName(RegistryItem.TargetClassPath), RegistryItem);
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

bool ReplicationRegistryUtils::PromptForSaveAndCloseRegistryTable()
{
	const UObject* RegistryTable = StaticFindObjectFast(
		nullptr, nullptr,
		*FString::Printf(TEXT("%s/%s"), *GetRegistryTablePackagePath(), *GetRegistryTableAssetName())
	);
	if (RegistryTable == nullptr)
	{
		return true;
	}
	if (RegistryTable->GetPackage()->IsDirty())
	{
		const FText SaveDialogText = LOCTEXT("SaveRegistryTableDialogText", "The DataTable: \"ReplicationRegistry\" has been modified. Do you want to save it?");
		const EAppReturnType::Type ReturnType = FMessageDialog::Open(EAppMsgType::OkCancel, SaveDialogText);
		if (ReturnType == EAppReturnType::Type::Ok)
		{
			return SaveRegistryPackage(RegistryTable->GetPackage());
		}
	}
	const FText HitDialogText = LOCTEXT("SaveRegistryTableDialogText", "Please save and close DataTable: \"ReplicationRegistry\" first, and then try again.");
	FMessageDialog::Open(EAppMsgType::Ok, HitDialogText);
	return false;
}

#undef LOCTEXT_NAMESPACE
