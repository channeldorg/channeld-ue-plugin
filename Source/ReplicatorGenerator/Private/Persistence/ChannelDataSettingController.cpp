// Fill out your copyright notice in the Description page of Project Settings.


#include "Persistence/ChannelDataSettingController.h"

#include "Persistence/RepActorCacheController.h"

void UChannelDataSettingController::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ChannelDataStateSettingModal.SetDataTableAssetName(TEXT("ChannelDataSetting"));
}

void UChannelDataSettingController::RefreshChannelDataStateOption()
{
	URepActorCacheController* RepActorCacheController = GEditor->GetEngineSubsystem<URepActorCacheController>();
	RepActorCacheController->LoadRepActorCache();
	TArray<FString> RepActorClassPaths;
	RepActorCacheController->GetRepActorClassPaths(RepActorClassPaths);
	ChannelDataStateOptions.Empty(RepActorClassPaths.Num());

	for (const FString& RepActorClassPath : RepActorClassPaths)
	{
		ChannelDataStateOptions.Add(FChannelDataStateOption(RepActorClassPath));
	}
}

void UChannelDataSettingController::GetChannelDataStateOptions(TArray<FChannelDataStateOption>& Options) const
{
	Options = ChannelDataStateOptions;
}

void UChannelDataSettingController::GetChannelDataSettings(TArray<FChannelDataSetting>& ChannelDataSettings)
{
	if (!ChannelDataStateSettingModal.IsExist())
	{
		GetDefaultChannelDataSettings(ChannelDataSettings);
		SaveChannelDataSettings(ChannelDataSettings);
	}
	else
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
	}
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

void UChannelDataSettingController::GetDefaultChannelDataSettings(TArray<FChannelDataSetting>& ChannelDataSettings)
{
	ChannelDataSettings.Empty();
	FChannelDataSetting DefaultChannelDataSetting(EChanneldChannelType::ECT_SubWorld, 1);
	TArray<FChannelDataStateSetting>& StateSettings = DefaultChannelDataSetting.StateSettings;
	TArray<FChannelDataStateOption> StateOptions;
	GetChannelDataStateOptions(StateOptions);
	int Index = 0;
	for (FChannelDataStateOption& StateOption : StateOptions)
	{
		StateSettings.Add(FChannelDataStateSetting(
			EChanneldChannelType::ECT_SubWorld,
			StateOption.ReplicationClassPath,
			++Index,
			false,
			false
		));
	}
	ChannelDataSettings.Add(DefaultChannelDataSetting);
}
