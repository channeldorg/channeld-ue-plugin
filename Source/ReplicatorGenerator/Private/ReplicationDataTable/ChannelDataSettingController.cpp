// Fill out your copyright notice in the Description page of Project Settings.


#include "ReplicationDataTable/ChannelDataSettingController.h"

void UChannelDataSettingController::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ReplicationRegistryController = &FReplicationRegistryController::Get();
	ChannelDataStateSettingModal.SetDataTableAssetName(TEXT("ChannelDataSetting"));

	RefreshChannelDataStateOption();
}

void UChannelDataSettingController::RefreshChannelDataStateOption()
{
	TArray<FChanneldReplicationRegistryRow*> RegistryTableRows;
	ReplicationRegistryController->GetItems_Unsafe(RegistryTableRows);
	ChannelDataStateOptions.Empty(RegistryTableRows.Num());

	for (const FChanneldReplicationRegistryRow* RegistryTableRow : RegistryTableRows)
	{
		FChannelDataStateOption NewOption(RegistryTableRow->TargetClassPath);
		ChannelDataStateOptions.Add(NewOption);
	}
}

void UChannelDataSettingController::GetChannelDataStateOptions(TArray<FChannelDataStateOption>& Options) const
{
	Options = ChannelDataStateOptions;
}

void UChannelDataSettingController::GetChannelDataSettings(TArray<FChannelDataSetting>& ChannelDataSettings)
{
	TArray<FChannelDataStateSettingRow*> DataStateSettingRows;
	ChannelDataStateSettingModal.GetItems_Unsafe(DataStateSettingRows);
	if (DataStateSettingRows.Num() == 0)
	{
		return;
	}
	TMap<int32, FChannelDataSetting> ChannelDataSettingMap;
	for (const FChannelDataStateSettingRow* DataStateSettingRow : DataStateSettingRows)
	{
		FChannelDataSetting* ChannelDataSetting;

		if ((ChannelDataSetting = ChannelDataSettingMap.Find(DataStateSettingRow->ChannelType)) == nullptr)
		{
			ChannelDataSettingMap.Add(
				DataStateSettingRow->ChannelType,
				FChannelDataSetting(DataStateSettingRow->ChannelType, DataStateSettingRow->ChannelTypeOrder)
			);
			ChannelDataSetting = ChannelDataSettingMap.Find(DataStateSettingRow->ChannelType);
		}
		ChannelDataSetting->StateSettings.Add(FChannelDataStateSetting(
			ChannelDataSetting->ChannelType
			, DataStateSettingRow->ReplicationClassPath
			, DataStateSettingRow->StateOrder
			, DataStateSettingRow->bSkip
			, DataStateSettingRow->bSingleton
		));
	}
	ChannelDataSettingMap.GenerateValueArray(ChannelDataSettings);
	ChannelDataSettings.Sort([](const FChannelDataSetting& A, const FChannelDataSetting& B)
	{
		return A.ChannelTypeOrder < B.ChannelTypeOrder;
	});
	for (FChannelDataSetting& DataSetting : ChannelDataSettings)
	{
		DataSetting.StateSettings.Sort([](const FChannelDataStateSetting& A, const FChannelDataStateSetting& B)
		{
			return A.StateOrder < B.StateOrder;
		});
	}
}

void UChannelDataSettingController::SaveChannelDataSettings(const TArray<FChannelDataSetting>& ChannelDataSettings)
{
	TArray<FChannelDataStateSettingRow> DataStateSettingRows;
	for (const FChannelDataSetting& ChannelDataSetting : ChannelDataSettings)
	{
		for (const FChannelDataStateSetting& StateSetting : ChannelDataSetting.StateSettings)
		{
			DataStateSettingRows.Add(FChannelDataStateSettingRow(
				static_cast<int32>(ChannelDataSetting.ChannelType),
				ChannelDataSetting.ChannelTypeOrder,
				StateSetting.ReplicationClassPath,
				StateSetting.StateOrder,
				StateSetting.bSkip,
				StateSetting.bSingleton
			));
		}
	}
	ChannelDataStateSettingModal.SetAll(DataStateSettingRows);
	ChannelDataStateSettingModal.SaveDataTable();
}
