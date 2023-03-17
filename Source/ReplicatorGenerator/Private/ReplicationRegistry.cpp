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
	UObject* RegistryTable = StaticFindObjectFast(
		nullptr, nullptr,
		*FString::Printf(TEXT("%s/%s"), *GetRegistryTablePackagePath(), *GetRegistryTableAssetName())
	);
	if (RegistryTable == nullptr)
	{
		return true;
	}
	if (UAssetEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
	{
		if (RegistryTable->GetPackage()->IsDirty())
		{
			const FText SaveDialogText = LOCTEXT("RegistryTableDialogText", "The DataTable: \"ReplicationRegistry\" has been modified. You must save it before you can continue. Do you want to save it now?");
			if (FMessageDialog::Open(EAppMsgType::OkCancel, SaveDialogText) == EAppReturnType::Type::Ok)
			{
				if (!SaveRegistryPackage(RegistryTable->GetPackage()))
				{
					FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("RegistryTableDialogText", "Failed to save DataTable: \"ReplicationRegistry\". Please try again."));
					return false;
				}
			}
		}
		const FText CloseDialogText = LOCTEXT("RegistryTableDialogText", "The DataTable: \"ReplicationRegistry\" is currently open in editor. You must close it before you can continue. Do you want to close it now?");
		if (FMessageDialog::Open(EAppMsgType::OkCancel, CloseDialogText) != EAppReturnType::Type::Ok)
		{
			return false;
		}
		EditorSubsystem->CloseAllEditorsForAsset(RegistryTable);
		// We need to collect garbage to make sure the ReplicationRegistryTable asset is released.
		CollectGarbage(RF_NoFlags);
		return true;
	}
	const FText HitDialogText = LOCTEXT("RegistryTableDialogText", "Please save and close DataTable: \"ReplicationRegistry\" first, and then try again.");
	FMessageDialog::Open(EAppMsgType::OkCancel, HitDialogText);
	return false;
}

#undef LOCTEXT_NAMESPACE
