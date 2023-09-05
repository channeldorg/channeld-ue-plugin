#include "Persistence/ChannelDataSchemaController.h"

#include "ScopedTransaction.h"
#include "Persistence/RepActorCacheController.h"

#define LOCTEXT_NAMESPACE "VictoryVertexSnapEditor"

void UChannelDataSchemaTransaction::PostEditUndo()
{
	UObject::PostEditUndo();
	PostDataSchemaUndoRedo.ExecuteIfBound();
}

void UChannelDataSchemaController::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// The "PLUGIN_DIR" is defined in the ReplicatorGenerator.Build.cs, but it is not available in the header file, so we have to use it here.
	DefaultChannelDataSchemaModel = FString(ANSI_TO_TCHAR(PLUGIN_DIR)) / TEXT("Config") / GenManager_DefaultChannelDataSchemataFile;

	ChannelDataSchemaTransaction = NewObject<UChannelDataSchemaTransaction>();
	ChannelDataSchemaTransaction->SetFlags(RF_Transactional);
	ChannelDataSchemaTransaction->PostDataSchemaUndoRedo.BindUObject(this, &UChannelDataSchemaController::HandlePostDataSchemaUndoRedo);
	TArray<FChannelDataSchema> ChannelDataSchemata;
	GetChannelDataSchemata(ChannelDataSchemata);
	ChannelDataSchemaTransaction->ChannelDataSchemata = ChannelDataSchemata;
}

void UChannelDataSchemaController::GetUnhiddenChannelTypes(TArray<EChanneldChannelType>& ChannelTypes) const
{
#if ENGINE_MAJOR_VERSION < 5
	const UEnum* EnumPtr = FindObject<UEnum>(ANY_PACKAGE, TEXT("EChanneldChannelType"), true);
#else
	const UEnum* EnumPtr = FindObject<UEnum>(FTopLevelAssetPath(TEXT("/Script/ChanneldUE.EChanneldChannelType")), true);
#endif
	if (!EnumPtr)
	{
		UE_LOG(LogChanneldRepGenerator, Error, TEXT("Failed to find enum EChanneldChannelType"));
		return;
	}
	ChannelTypes.Empty();
	for (int32 i = 0; i < EnumPtr->NumEnums(); ++i)
	{
		// Hide the spatial channel type for the schema editor.
		if (EnumPtr->GetValueByIndex(i) == static_cast<int64>(EChanneldChannelType::ECT_Spatial))
		{
			continue;
		}
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
	if (!ChannelDataSchemaModel.IsExist())
	{
		GetDefaultChannelDataSchemata(ChannelDataSchemata);
	}
	else
	{
		ChannelDataSchemaModel.GetDataArray(ChannelDataSchemata);

		// v0.6 upgrade: remove the spatial channel data schema.
		for (int32 i = 0; i < ChannelDataSchemata.Num(); ++i)
		{
			if (ChannelDataSchemata[i].ChannelType == EChanneldChannelType::ECT_Spatial)
			{
				ChannelDataSchemata.RemoveAt(i);
				break;
			}
		}
	}
	SortChannelDataSchemata(ChannelDataSchemata);
}

void UChannelDataSchemaController::SaveChannelDataSchemata(const TArray<FChannelDataSchema>& ChannelDataSchemata)
{
	TArray<FChannelDataSchema>UpdatedChannelDataSchemata = ChannelDataSchemata;
	for(int32 I = 0; I< UpdatedChannelDataSchemata.Num(); ++I)
	{
		UpdatedChannelDataSchemata[I].ChannelTypeOrder = I+1;
		for(int32 J = 0; J < UpdatedChannelDataSchemata[I].StateSchemata.Num(); ++J)
		{
			UpdatedChannelDataSchemata[I].StateSchemata[J].StateOrder = J+1;
		}
	}
	
	// Save the channel data schemata to the json file.
	ChannelDataSchemaModel.SaveDataArray(UpdatedChannelDataSchemata);

	// Save the channel data schemata to the transaction.
	FScopedTransaction Transaction(TEXT("Replciation Generator"),LOCTEXT("SaveChannelDataSchemata", "Save Channel Data Schemata"), ChannelDataSchemaTransaction);
	ChannelDataSchemaTransaction->Modify();
	ChannelDataSchemaTransaction->ChannelDataSchemata = UpdatedChannelDataSchemata;

	PostDataSchemaUpdated.Broadcast(UpdatedChannelDataSchemata);
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
	DefaultChannelDataSchemaModel.GetDataArray(ChannelDataSchemata);
}

FDateTime UChannelDataSchemaController::GetLastUpdatedTime() const
{
	return ChannelDataSchemaModel.LastUpdatedTime();
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

void UChannelDataSchemaController::HandlePostDataSchemaUndoRedo()
{
	ChannelDataSchemaModel.SaveDataArray(ChannelDataSchemaTransaction->ChannelDataSchemata);
	PostDataSchemaUpdated.Broadcast(ChannelDataSchemaTransaction->ChannelDataSchemata);
}

#undef LOCTEXT_NAMESPACE
