// Fill out your copyright notice in the Description page of Project Settings.


#include "Commandlets/CookAndFilterRepActorCommandlet.h"

#include "ReplicatorGeneratorDefinition.h"
#include "ReplicatorGeneratorManager.h"
#include "ReplicatorGeneratorUtils.h"
#include "Misc/FileHelper.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UCookAndFilterRepActorCommandlet::UCookAndFilterRepActorCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCookAndFilterRepActorCommandlet::Main(const FString& CmdLineParams)
{
	FReplicatorGeneratorManager& GeneratorManager = FReplicatorGeneratorManager::Get();
	GeneratorManager.StartGenerateReplicator();
	
	ChanneldReplicatorGeneratorUtils::FObjectLoadedListener ObjLoadedListener;
	ObjLoadedListener.StartListen();
	
	const FString AdditionalParam(TEXT(" -SkipShaderCompile"));
	FString NewCmdLine = CmdLineParams;
	NewCmdLine.Append(AdditionalParam);
	FCommandLine::Append(*AdditionalParam);
	
	int32 Result = Super::Main(CmdLineParams);
	ObjLoadedListener.StopListen();
	
	TArray<FString> LoadedRepClassPath;
	for (const FSoftClassPath& ObjSoftPath : ObjLoadedListener.LoadedRepClasses)
	{
		FString AssetPath = ObjSoftPath.GetAssetPathName().ToString();
		// AssetPath.RemoveFromEnd(TEXT("_C"));
		LoadedRepClassPath.Add(AssetPath);
	}
	SaveResult(LoadedRepClassPath);

	return Result;
}

void UCookAndFilterRepActorCommandlet::LoadResult(TArray<FString>& Result, bool& Success)
{
	FString Json;
	Success = false;

	if (!FFileHelper::LoadFileToString(Json, *GenManager_RepClassInfoPath))
	{
		UE_LOG(LogChanneldRepGenerator, Error, TEXT("Unable to load RepClassInfo: %s"), *GenManager_RepClassInfoPath);
		return;
	}

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TSharedPtr<FJsonObject> RootObject;
	bool bSuccessful = FJsonSerializer::Deserialize(Reader, RootObject);
	if (bSuccessful)
	{
		const TArray<TSharedPtr<FJsonValue>>* RepAssets;
		if (RootObject->TryGetArrayField(TEXT("RepClass"), RepAssets))
		{
			for (int32 i = 0; i < (*RepAssets).Num(); ++i)
			{
				Result.Add((*RepAssets)[i]->AsString());
			}
		}
		Success = true;
	}
	return;
}

void UCookAndFilterRepActorCommandlet::SaveResult(TArray<FString> RepAssets)
{
	FString Json;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);

	JsonWriter->WriteObjectStart();
	JsonWriter->WriteValue(L"RepClass", RepAssets);
	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();
	if (!FFileHelper::SaveStringToFile(Json, *GenManager_RepClassInfoPath))
	{
		UE_LOG(LogChanneldRepGenerator, Error, TEXT("Unable to save RepClassInfo: %s"), *GenManager_RepClassInfoPath);
	}
}
