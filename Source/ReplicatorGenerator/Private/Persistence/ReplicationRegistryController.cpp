// Fill out your copyright notice in the Description page of Project Settings.


#include "Persistence/ReplicationRegistryController.h"

#define LOCTEXT_NAMESPACE "ReplicationRegistryController"

FReplicationRegistryController::FReplicationRegistryController()
{
	RepRegistryModal.SetDataTableAssetName(TEXT("ReplicationRegistry"));
}

FReplicationRegistryController& FReplicationRegistryController::Get()
{
	static FReplicationRegistryController Instance;
	return Instance;
}

bool FReplicationRegistryController::GetItems(TArray<FChanneldReplicationRegistryRow*>& OutItems)
{
	return RepRegistryModal.GetItems(OutItems);
}

void FReplicationRegistryController::GetItems_Unsafe(TArray<FChanneldReplicationRegistryRow*>& OutItems)
{
	RepRegistryModal.GetItems_Unsafe(OutItems);
}

bool FReplicationRegistryController::UpdateRegistryTable(const TArray<FChanneldReplicationRegistryRow>& ItemsToAdd, const TArray<FString>& ItemsToRemove)
{
	if (!RepRegistryModal.AddItems(ItemsToAdd))
	{
		return false;
	}
	TArray<FName> ItemsToRemoveRowName;
	for (const FString& ItemToRemove : ItemsToRemove)
	{
		ItemsToRemoveRowName.Add(FName(ItemToRemove));
	}
	if (!RepRegistryModal.RemoveItems(ItemsToRemoveRowName))
	{
		return false;
	}
	if (!RepRegistryModal.SaveDataTable())
	{
		return false;
	}
	return true;
}

bool FReplicationRegistryController::PromptForSaveAndCloseRegistryTable()
{
	CollectGarbage(RF_NoFlags);
	UObject* RegistryTable = StaticFindObjectFast(
		nullptr, nullptr,
		*FString::Printf(TEXT("%s/%s"), *RepRegistryModal.GetDataTableOuterPackagePath(), *RepRegistryModal.GetDataTableAssetName())
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
				if (!RepRegistryModal.SaveDataTable())
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
