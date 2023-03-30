#include "Persistence/JsonModel.h"
#include "ReplicatorGeneratorDefinition.h"
#include "JsonObjectConverter.h"

template <typename OutStructType>
TJsonModel<OutStructType>::TJsonModel(const FString& InDataFilePath)
{
	SetDataFilePath(InDataFilePath);
}

template <typename OutStructType>
void TJsonModel<OutStructType>::SetDataFilePath(const FString& InDataFilePath)
{
	DataFilePath = InDataFilePath;
	FPaths::NormalizeFilename(DataFilePath);
}

template <typename OutStructType>
bool TJsonModel<OutStructType>::IsExist() const
{
	return FPaths::FileExists(DataFilePath);
}

template <typename OutStructType>
bool TJsonModel<OutStructType>::GetData(OutStructType& OutData, bool ForceLoad)
{
	OutData = OutStructType();
	if (ForceLoad || IsNewer())
	{
		if (!LoadData())
		{
			return false;
		}
	}
	OutData = Data;
	return true;
}

template <typename OutStructType>
bool TJsonModel<OutStructType>::GetDataArray(TArray<OutStructType>& OutData, bool ForceLoad)
{
	OutData.Empty();
	if (ForceLoad || IsNewer())
	{
		if (!LoadDataArray())
		{
			return false;
		}
	}
	OutData = DataArray;
	return true;
}

template <typename OutStructType>
bool TJsonModel<OutStructType>::LoadData()
{
	if (!IsExist())
	{
		Data = OutStructType();
		bLastLoadExisting = false;
	}
	else
	{
		FString JsonString;
		if (!FFileHelper::LoadFileToString(JsonString, *DataFilePath))
		{
			UE_LOG(LogChanneldRepGenerator, Error, TEXT("Failed to load data from file: %s"), *DataFilePath);
			return false;
		}
		if (!FJsonObjectConverter::JsonObjectStringToUStruct<OutStructType>(JsonString, &Data, 0, 0))
		{
			UE_LOG(LogChanneldRepGenerator, Error, TEXT("Failed to parse json from file: %s"), *DataFilePath);
			return false;
		}
		bLastLoadExisting = true;
	}
	LastLoadTime = FDateTime::UtcNow();
	return true;
}

template <typename OutStructType>
bool TJsonModel<OutStructType>::LoadDataArray()
{
	if (!IsExist())
	{
		DataArray.Empty();
		bLastLoadExisting = false;
	}
	else
	{
		FString JsonString;
		if (!FFileHelper::LoadFileToString(JsonString, *DataFilePath))
		{
			UE_LOG(LogChanneldRepGenerator, Error, TEXT("Failed to load data from file: %s"), *DataFilePath);
			return false;
		}
		if (!FJsonObjectConverter::JsonArrayStringToUStruct<OutStructType>(JsonString, &DataArray, 0, 0))
		{
			UE_LOG(LogChanneldRepGenerator, Error, TEXT("Failed to parse json from file: %s"), *DataFilePath);
			return false;
		}
		bLastLoadExisting = true;
	}
	LastLoadTime = FDateTime::UtcNow();
	return true;
}

template <typename OutStructType>
bool TJsonModel<OutStructType>::SaveData(const OutStructType& InData)
{
	FString JsonString;
	if (!FJsonObjectConverter::UStructToJsonObjectString(InData, JsonString, 0, 0))
	{
		UE_LOG(LogChanneldRepGenerator, Error, TEXT("Failed to convert data to json: %s"), *DataFilePath);
		return false;
	}
	if (!FFileHelper::SaveStringToFile(JsonString, *DataFilePath))
	{
		UE_LOG(LogChanneldRepGenerator, Error, TEXT("Failed to save data to file: %s"), *DataFilePath);
		return false;
	}
	return true;
}

template <typename OutStructType>
bool TJsonModel<OutStructType>::SaveDataArray(const TArray<OutStructType>& InDataArray)
{
	FString JsonArrayString;
	for (const OutStructType& InData : InDataArray)
	{
		FString JsonString;
		if (!FJsonObjectConverter::UStructToJsonObjectString(InData, JsonString, 0, 0))
		{
			UE_LOG(LogChanneldRepGenerator, Warning, TEXT("Failed to convert data to json: %s"), *DataFilePath);
			continue;
		}
		if (!JsonArrayString.IsEmpty())
		{
			JsonArrayString.Append(TEXT(",\n"));
		}
		JsonArrayString.Append(JsonString);
	}
	if (!FFileHelper::SaveStringToFile(FString::Printf(TEXT("[\n%s\n]"), *JsonArrayString), *DataFilePath))
	{
		UE_LOG(LogChanneldRepGenerator, Error, TEXT("Failed to save data to file: %s"), *DataFilePath);
		return false;
	}
	return true;
}

template <typename OutStructType>
bool TJsonModel<OutStructType>::IsNewer() const
{
	if (bLastLoadExisting && !IsExist())
	{
		return true;
	}
	return IFileManager::Get().GetTimeStamp(*DataFilePath) > LastLoadTime;
}
