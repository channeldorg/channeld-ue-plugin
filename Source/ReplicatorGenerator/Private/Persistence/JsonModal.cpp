#include "Persistence/JsonModel.h"
#include "ReplicatorGeneratorDefinition.h"
#include "JsonObjectConverter.h"

template <typename OutStructType>
void TJsonModel<OutStructType>::SetDataFilePath(const FString& InDataFilePath)
{
	DataFilePath = InDataFilePath;
	FPaths::NormalizeFilename(DataFilePath);
}

template <typename OutStructType>
void TJsonModel<OutStructType>::LoadData(OutStructType& OutData)
{
	FString JsonString;
	FFileHelper::LoadFileToString(JsonString, *DataFilePath);
	FJsonObjectConverter::JsonObjectStringToUStruct<OutStructType>(JsonString, &OutData, 0, 0);
}

template <typename OutStructType>
void TJsonModel<OutStructType>::LoadData(TArray<OutStructType>& OutData)
{
	FString JsonString;
	FFileHelper::LoadFileToString(JsonString, *DataFilePath);
	FJsonObjectConverter::JsonArrayStringToUStruct<OutStructType>(JsonString, &OutData, 0, 0);
}

template <typename OutStructType>
bool TJsonModel<OutStructType>::SaveData(const OutStructType& InData)
{
	FString JsonString;
	if (!FJsonObjectConverter::UStructToJsonObjectString(InData, JsonString, 0, 0)) return;
	if(FFileHelper::SaveStringToFile(JsonString, *DataFilePath))
	{
		return true;
	}
	UE_LOG(LogChanneldRepGenerator, Error, TEXT("Failed to save data to file: %s"), *DataFilePath);
	return false;
}

template <typename OutStructType>
bool TJsonModel<OutStructType>::SaveData(const TArray<OutStructType>& InDataArray)
{
	FString JsonArrayString;
	for (const OutStructType& InData : InDataArray)
	{
		FString JsonString;
		if (!FJsonObjectConverter::UStructToJsonObjectString(InData, JsonString, 0, 0)) continue;
		if (!JsonArrayString.IsEmpty())
		{
			JsonArrayString.Append(TEXT(",\n"));
		}
		JsonArrayString.Append(JsonString);
	}
	if(FFileHelper::SaveStringToFile(FString::Printf(TEXT("[\n%s\n]"), *JsonArrayString), *DataFilePath))
	{
		return true;
	}
	UE_LOG(LogChanneldRepGenerator, Error, TEXT("Failed to save data to file: %s"), *DataFilePath);
	return false;
}
