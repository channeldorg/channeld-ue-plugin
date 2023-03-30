#pragma once

template <typename OutStructType>
class TJsonModel
{
protected:
	FString DataFilePath;

	FDateTime LastLoadTime;
	bool bLastLoadExisting = true;

	OutStructType Data;

	TArray<OutStructType> DataArray;

public:
	TJsonModel() = default;

	TJsonModel(const FString& InDataFilePath);

	void SetDataFilePath(const FString& InDataFilePath);

	bool IsExist() const;

	bool GetData(OutStructType& OutData, bool ForceLoad = false);

	bool GetDataArray(TArray<OutStructType>& OutData, bool ForceLoad = false);

	bool LoadData();

	bool LoadDataArray();

	bool SaveData(const OutStructType& InData);

	bool SaveDataArray(const TArray<OutStructType>& InDataArray);

	bool IsNewer() const;
};
