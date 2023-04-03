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
		ChannelDataStateSchemaModal.GetDataArray(ChannelDataSchemata);
	}
}

void UChannelDataSchemaController::SaveChannelDataSchemata(const TArray<FChannelDataSchema>& ChannelDataSchemata)
{
	ChannelDataStateSchemaModal.SaveDataArray(ChannelDataSchemata);
}

void UChannelDataSchemaController::ImportChannelDataSchemataFrom(const FString& FilePath, TArray<FChannelDataSchema>& ChannelDataSchemata, bool& Success)
{
	TJsonModel<FChannelDataSchema> TmpModel(FilePath);
	if (!TmpModel.GetDataArray(ChannelDataSchemata))
	{
		Success = false;
		return;
	}
	Success = true;
}

void UChannelDataSchemaController::ExportChannelDataSchemataTo(const FString& FilePath, const TArray<FChannelDataSchema>& ChannelDataSchemata, bool& Success)
{
	TJsonModel<FChannelDataSchema> TmpModel(FilePath);
	if (!TmpModel.SaveDataArray(ChannelDataSchemata))
	{
		Success = false;
		return;
	}
	Success = true;
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
	URepActorCacheController* RepActorCacheController = GEditor->GetEditorSubsystem<URepActorCacheController>();

	int Index = 0;
	for (FChannelDataStateOption& StateOption : StateOptions)
	{
		StateSchemata.Add(FChannelDataStateSchema(
			EChanneldChannelType::ECT_SubWorld
			, StateOption.ReplicationClassPath
			, ++Index
			, false
			, RepActorCacheController->IsDefaultSingleton(StateOption.ReplicationClassPath)
		));
	}
	ChannelDataSchemata.Add(DefaultChannelDataSchema);
	SortChannelDataSchemata(ChannelDataSchemata);
}
