#pragma once

template <typename OutStructType>
class TJsonModel
{
protected:
	FString DataFilePath;

public:
	TJsonModel() = default;

	void SetDataFilePath(const FString& InDataFilePath);

	void LoadData(OutStructType& OutData);

	void LoadData(TArray<OutStructType>& OutData);

	bool SaveData(const OutStructType& InData);

	bool SaveData(const TArray<OutStructType>& InDataArray);
};
