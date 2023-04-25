#include "Persistence/ChannelDataSchemaController.h"

#include "ReplicatorGeneratorUtils.h"
#include "Persistence/RepActorCacheController.h"

void UChannelDataSchemaController::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// The "PLUGIN_DIR" is defined in the ReplicatorGenerator.Build.cs, but it is not available in the header file, so we have to use it here.
	DefaultChannelDataSchemaModal = FString(ANSI_TO_TCHAR(PLUGIN_DIR)) / TEXT("Config") / GenManager_DefaultChannelDataSchemataFile;
}

void UChannelDataSchemaController::GetUnhiddenChannelTypes(TArray<EChanneldChannelType>& ChannelTypes) const
{
	const UEnum* EnumPtr = FindObject<UEnum>(ANY_PACKAGE, TEXT("EChanneldChannelType"), true);
	if (!EnumPtr)
	{
		UE_LOG(LogChanneldRepGenerator, Error, TEXT("Failed to find enum EChanneldChannelType"));
		return;
	}
	ChannelTypes.Empty();
	for (int32 i = 0; i < EnumPtr->NumEnums(); ++i)
	{
		if (EnumPtr->HasMetaData(TEXT("Hidden"), i))
		{
			continue;
		}
		ChannelTypes.Add(static_cast<EChanneldChannelType>(EnumPtr->GetValueByIndex(i)));
	}
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
	if (!ChannelDataSchemaModal.IsExist())
	{
		GetDefaultChannelDataSchemata(ChannelDataSchemata);
	}
	else
	{
		ChannelDataSchemaModal.GetDataArray(ChannelDataSchemata);
	}
	SortChannelDataSchemata(ChannelDataSchemata);
}

void UChannelDataSchemaController::SaveChannelDataSchemata(const TArray<FChannelDataSchema>& ChannelDataSchemata)
{
	ChannelDataSchemaModal.SaveDataArray(ChannelDataSchemata);
}

void UChannelDataSchemaController::ImportChannelDataSchemataFrom(const FString& FilePath, TArray<FChannelDataSchema>& ChannelDataSchemata, bool& Success)
{
	TJsonModel<FChannelDataSchema> TmpModel(FilePath);
	if (!TmpModel.GetDataArray(ChannelDataSchemata))
	{
		Success = false;
		return;
	}
	SortChannelDataSchemata(ChannelDataSchemata);
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

void UChannelDataSchemaController::GetDefaultChannelDataSchemata(TArray<FChannelDataSchema>& ChannelDataSchemata)
{
	DefaultChannelDataSchemaModal.GetDataArray(ChannelDataSchemata);
}

FDateTime UChannelDataSchemaController::GetLastUpdatedTime() const
{
	return ChannelDataSchemaModal.LastUpdatedTime();
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
