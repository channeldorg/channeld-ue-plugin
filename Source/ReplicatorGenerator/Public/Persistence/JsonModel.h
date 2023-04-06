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
	virtual ~TJsonModel() = default;

	virtual void SetDataFilePath(const FString& InDataFilePath);

	virtual bool IsExist() const;

	virtual bool GetData(OutStructType& OutData, bool ForceLoad = false);

	virtual bool GetDataArray(TArray<OutStructType>& OutData, bool ForceLoad = false);

	virtual bool LoadData();

	virtual bool LoadDataArray();

	virtual bool SaveData(const OutStructType& InData);

	virtual bool SaveDataArray(const TArray<OutStructType>& InDataArray);

	virtual bool IsNewer() const;
};
