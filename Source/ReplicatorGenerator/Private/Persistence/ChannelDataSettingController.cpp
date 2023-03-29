#include "Persistence/ChannelDataSettingController.h"
#include "ReplicatorGeneratorDefinition.h"
#include "Persistence/RepActorCacheController.h"

void UChannelDataSettingController::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ChannelDataStateSettingModal.SetDataFilePath(GenManager_ChannelDataSettingsPath);
}

void UChannelDataSettingController::GetChannelDataStateOptions(TArray<FChannelDataStateOption>& Options) const
{
	URepActorCacheController* RepActorCacheController = GEditor->GetEngineSubsystem<URepActorCacheController>();
	TArray<FString> RepActorClassPaths;
	RepActorCacheController->GetRepActorClassPaths(RepActorClassPaths);

	Options.Empty(RepActorClassPaths.Num());
	for (const FString& RepActorClassPath : RepActorClassPaths)
	{
		Options.Add(FChannelDataStateOption(RepActorClassPath));
	}
}

void UChannelDataSettingController::GetChannelDataSettings(TArray<FChannelDataSetting>& ChannelDataSettings)
{
	if (!ChannelDataStateSettingModal.IsExist())
	{
		GetDefaultChannelDataSettings(ChannelDataSettings);
		// SaveChannelDataSettings(ChannelDataSettings);
	}
	else
	{
		TArray<FChannelDataStateSettingRow> DataStateSettingRows;
		ChannelDataStateSettingModal.GetDataArray(DataStateSettingRows);

		if (DataStateSettingRows.Num() == 0)
		{
			ChannelDataSettings.Empty();
			return;
		}
		ChannelDataSettings = ConvertRowsToChannelDataSettings(DataStateSettingRows);
	}
}

void UChannelDataSettingController::SaveChannelDataSettings(const TArray<FChannelDataSetting>& ChannelDataSettings)
{
	ChannelDataStateSettingModal.SaveDataArray(ConvertChannelDataSettingsToRows(ChannelDataSettings));
}

void UChannelDataSettingController::ImportChannelDataSettingsFrom(const FString& FilePath, TArray<FChannelDataSetting>& ChannelDataSettings, bool& Success)
{
	TJsonModel<FChannelDataStateSettingRow> TmpModel;
	TmpModel.SetDataFilePath(FilePath);
	TArray<FChannelDataStateSettingRow> DataStateSettingRows;
	if (!TmpModel.GetDataArray(DataStateSettingRows))
	{
		Success = false;
		return;
	}
	ChannelDataSettings = ConvertRowsToChannelDataSettings(DataStateSettingRows);
	Success = true;
}

void UChannelDataSettingController::ExportChannelDataSettingsTo(const FString& FilePath, const TArray<FChannelDataSetting>& ChannelDataSettings, bool& Success)
{
	TJsonModel<FChannelDataStateSettingRow> TmpModel;
	TmpModel.SetDataFilePath(FilePath);
	if (!TmpModel.SaveDataArray(ConvertChannelDataSettingsToRows(ChannelDataSettings)))
	{
		Success = false;
		return;
	}
	Success = true;
}

TArray<FChannelDataStateSettingRow> UChannelDataSettingController::ConvertChannelDataSettingsToRows(const TArray<FChannelDataSetting>& ChannelDataSettings)
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
	return DataStateSettingRows;
}

inline TArray<FChannelDataSetting> UChannelDataSettingController::ConvertRowsToChannelDataSettings(const TArray<FChannelDataStateSettingRow>& ChannelDataSettingRows)
{
	TArray<FChannelDataSetting> ChannelDataSettings;
	TMap<int32, FChannelDataSetting> ChannelDataSettingMap;
	for (const FChannelDataStateSettingRow& DataStateSettingRow : ChannelDataSettingRows)
	{
		FChannelDataSetting* ChannelDataSetting;

		if ((ChannelDataSetting = ChannelDataSettingMap.Find(DataStateSettingRow.ChannelType)) == nullptr)
		{
			ChannelDataSettingMap.Add(
				DataStateSettingRow.ChannelType,
				FChannelDataSetting(DataStateSettingRow.ChannelType, DataStateSettingRow.ChannelTypeOrder)
			);
			ChannelDataSetting = ChannelDataSettingMap.Find(DataStateSettingRow.ChannelType);
		}
		ChannelDataSetting->StateSettings.Add(FChannelDataStateSetting(
			ChannelDataSetting->ChannelType
			, DataStateSettingRow.ReplicationClassPath
			, DataStateSettingRow.StateOrder
			, DataStateSettingRow.bSkip
			, DataStateSettingRow.bSingleton
		));
	}
	ChannelDataSettingMap.GenerateValueArray(ChannelDataSettings);
	SortChannelDataSettings(ChannelDataSettings);
	return ChannelDataSettings;
}

void UChannelDataSettingController::SortChannelDataSettings(TArray<FChannelDataSetting>& ChannelDataSettings)
{
	ChannelDataSettings.Sort([](const FChannelDataSetting& A, const FChannelDataSetting& B)
	{
		return A.ChannelTypeOrder < B.ChannelTypeOrder;
	});
	for (FChannelDataSetting& DataSetting : ChannelDataSettings)
	{
		DataSetting.Sort();
	}
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
	SortChannelDataSettings(ChannelDataSettings);
}
