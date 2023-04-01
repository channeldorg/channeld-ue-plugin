#include "Persistence/ChannelDataSchemaController.h"
#include "Persistence/RepActorCacheController.h"

void UChannelDataSchemaController::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UChannelDataSchemaController::GetChannelDataStateOptions(TArray<FChannelDataStateOption>& Options) const
{
	URepActorCacheController* RepActorCacheController = GEditor->GetEditorSubsystem<URepActorCacheController>();
	TArray<FString> RepActorClassPaths;
	RepActorCacheController->GetRepActorClassPaths(RepActorClassPaths);

	Options.Empty(RepActorClassPaths.Num());
	for (const FString& RepActorClassPath : RepActorClassPaths)
	{
		Options.Add(FChannelDataStateOption(RepActorClassPath));
	}
}

void UChannelDataSchemaController::GetChannelDataSchemata(TArray<FChannelDataSchema>& ChannelDataSchemata)
{
	if (!ChannelDataStateSchemaModal.IsExist())
	{
		GetDefaultChannelDataSchemata(ChannelDataSchemata);
		// SaveChannelDataSchemata(ChannelDataSchemata);
	}
	else
	{
		TArray<FChannelDataStateSchemaRow> DataStateSchemaRows;
		ChannelDataStateSchemaModal.GetDataArray(DataStateSchemaRows);

		if (DataStateSchemaRows.Num() == 0)
		{
			ChannelDataSchemata.Empty();
			return;
		}
		ChannelDataSchemata = ConvertRowsToChannelDataSchemata(DataStateSchemaRows);
	}
}

void UChannelDataSchemaController::SaveChannelDataSchemata(const TArray<FChannelDataSchema>& ChannelDataSchemata)
{
	ChannelDataStateSchemaModal.SaveDataArray(ConvertChannelDataSchemataToRows(ChannelDataSchemata));
}

void UChannelDataSchemaController::ImportChannelDataSchemataFrom(const FString& FilePath, TArray<FChannelDataSchema>& ChannelDataSchemata, bool& Success)
{
	TJsonModel<FChannelDataStateSchemaRow> TmpModel(FilePath);
	TArray<FChannelDataStateSchemaRow> DataStateSchemaRows;
	if (!TmpModel.GetDataArray(DataStateSchemaRows))
	{
		Success = false;
		return;
	}
	ChannelDataSchemata = ConvertRowsToChannelDataSchemata(DataStateSchemaRows);
	Success = true;
}

void UChannelDataSchemaController::ExportChannelDataSchemataTo(const FString& FilePath, const TArray<FChannelDataSchema>& ChannelDataSchemata, bool& Success)
{
	TJsonModel<FChannelDataStateSchemaRow> TmpModel(FilePath);
	if (!TmpModel.SaveDataArray(ConvertChannelDataSchemataToRows(ChannelDataSchemata)))
	{
		Success = false;
		return;
	}
	Success = true;
}

TArray<FChannelDataStateSchemaRow> UChannelDataSchemaController::ConvertChannelDataSchemataToRows(const TArray<FChannelDataSchema>& ChannelDataSchemata)
{
	TArray<FChannelDataStateSchemaRow> DataStateSchemaRows;
	for (const FChannelDataSchema& ChannelDataSchema : ChannelDataSchemata)
	{
		for (const FChannelDataStateSchema& StateSchema : ChannelDataSchema.StateSchemata)
		{
			DataStateSchemaRows.Add(FChannelDataStateSchemaRow(
				static_cast<int32>(ChannelDataSchema.ChannelType),
				ChannelDataSchema.ChannelTypeOrder,
				StateSchema.ReplicationClassPath,
				StateSchema.StateOrder,
				StateSchema.bSkip,
				StateSchema.bSingleton
			));
		}
	}
	return DataStateSchemaRows;
}

inline TArray<FChannelDataSchema> UChannelDataSchemaController::ConvertRowsToChannelDataSchemata(const TArray<FChannelDataStateSchemaRow>& ChannelDataSchemaRows)
{
	TArray<FChannelDataSchema> ChannelDataSchemata;
	TMap<int32, FChannelDataSchema> ChannelDataSchemaMap;
	for (const FChannelDataStateSchemaRow& DataStateSchemaRow : ChannelDataSchemaRows)
	{
		FChannelDataSchema* ChannelDataSchema;

		if ((ChannelDataSchema = ChannelDataSchemaMap.Find(DataStateSchemaRow.ChannelType)) == nullptr)
		{
			ChannelDataSchemaMap.Add(
				DataStateSchemaRow.ChannelType,
				FChannelDataSchema(DataStateSchemaRow.ChannelType, DataStateSchemaRow.ChannelTypeOrder)
			);
			ChannelDataSchema = ChannelDataSchemaMap.Find(DataStateSchemaRow.ChannelType);
		}
		ChannelDataSchema->StateSchemata.Add(FChannelDataStateSchema(
			ChannelDataSchema->ChannelType
			, DataStateSchemaRow.ReplicationClassPath
			, DataStateSchemaRow.StateOrder
			, DataStateSchemaRow.bSkip
			, DataStateSchemaRow.bSingleton
		));
	}
	ChannelDataSchemaMap.GenerateValueArray(ChannelDataSchemata);
	SortChannelDataSchemata(ChannelDataSchemata);
	return ChannelDataSchemata;
}

void UChannelDataSchemaController::SortChannelDataSchemata(TArray<FChannelDataSchema>& ChannelDataSchemata)
{
	ChannelDataSchemata.Sort([](const FChannelDataSchema& A, const FChannelDataSchema& B)
	{
		return A.ChannelTypeOrder < B.ChannelTypeOrder;
	});
	for (FChannelDataSchema& DataSchema : ChannelDataSchemata)
	{
		DataSchema.Sort();
	}
}

void UChannelDataSchemaController::GetDefaultChannelDataSchemata(TArray<FChannelDataSchema>& ChannelDataSchemata)
{
	ChannelDataSchemata.Empty();
	FChannelDataSchema DefaultChannelDataSchema(EChanneldChannelType::ECT_SubWorld, 1);
	TArray<FChannelDataStateSchema>& StateSchemata = DefaultChannelDataSchema.StateSchemata;
	TArray<FChannelDataStateOption> StateOptions;
	GetChannelDataStateOptions(StateOptions);
	int Index = 0;
	for (FChannelDataStateOption& StateOption : StateOptions)
	{
		StateSchemata.Add(FChannelDataStateSchema(
			EChanneldChannelType::ECT_SubWorld,
			StateOption.ReplicationClassPath,
			++Index,
			false,
			false
		));
	}
	ChannelDataSchemata.Add(DefaultChannelDataSchema);
	SortChannelDataSchemata(ChannelDataSchemata);
}
