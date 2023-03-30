#pragma once

#include "CoreMinimal.h"
#include "ChanneldTypes.h"
#include "JsonModel.h"
#include "ReplicatorGeneratorDefinition.h"
#include "ChannelDataSettingController.generated.h"

USTRUCT(BlueprintType)
struct REPLICATORGENERATOR_API FChannelDataStateSettingRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 ChannelType = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 ChannelTypeOrder = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FString ReplicationClassPath;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 StateOrder = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bSkip = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bSingleton = false;

	FChannelDataStateSettingRow() = default;

	FChannelDataStateSettingRow(int32 InChannelType, int32 InChannelTypeOrder, const FString& InReplicationClassPath, int32 InStateOrder, bool InbSkip, bool InbSingleton)
		: ChannelType(InChannelType)
		  , ChannelTypeOrder(InChannelTypeOrder)
		  , ReplicationClassPath(InReplicationClassPath)
		  , StateOrder(InStateOrder)
		  , bSkip(InbSkip)
		  , bSingleton(InbSingleton)
	{
	}

	virtual FName GetRowName() const
	{
		return FName(FString::Printf(TEXT("%d$$%s"), ChannelType, *ReplicationClassPath));
	}
};

USTRUCT(BlueprintType)
struct REPLICATORGENERATOR_API FChannelDataStateSetting
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	EChanneldChannelType ChannelType = EChanneldChannelType::ECT_Unknown;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	FString ReplicationClassPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 StateOrder = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bSkip = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bSingleton = false;

	FChannelDataStateSetting() = default;

	FChannelDataStateSetting(EChanneldChannelType InChannelType, const FString& InReplicationClassPath, int32 InStateOrder)
		: ChannelType(InChannelType)
		  , ReplicationClassPath(InReplicationClassPath)
		  , StateOrder(InStateOrder)
	{
	}

	FChannelDataStateSetting(EChanneldChannelType InChannelType, const FString& InReplicationClassPath, int32 InStateOrder, bool InbSkip, bool InbSingleton)
		: ChannelType(InChannelType)
		  , ReplicationClassPath(InReplicationClassPath)
		  , StateOrder(InStateOrder)
		  , bSkip(InbSkip)
		  , bSingleton(InbSingleton)
	{
	}
};

USTRUCT(BlueprintType)
struct REPLICATORGENERATOR_API FChannelDataSetting
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EChanneldChannelType ChannelType = EChanneldChannelType::ECT_Unknown;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 ChannelTypeOrder = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FChannelDataStateSetting> StateSettings;

	FChannelDataSetting() = default;

	FChannelDataSetting(EChanneldChannelType InChannelType, int32 InChannelTypeOrder)
		: ChannelType(InChannelType)
		  , ChannelTypeOrder(InChannelTypeOrder)
	{
	}

	FChannelDataSetting(int32 InChannelType, int32 InChannelTypeOrder)
		: ChannelType(static_cast<EChanneldChannelType>(InChannelType))
		  , ChannelTypeOrder(InChannelTypeOrder)
	{
	}

	void Sort()
	{
		StateSettings.Sort([](const FChannelDataStateSetting& A, const FChannelDataStateSetting& B)
		{
			return A.StateOrder < B.StateOrder;
		});
	}
};

USTRUCT(BlueprintType)
struct REPLICATORGENERATOR_API FChannelDataStateOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	FString ReplicationClassPath;

	FChannelDataStateOption() = default;

	FChannelDataStateOption(const FString& InReplicationClassPath) : ReplicationClassPath(InReplicationClassPath)
	{
	}
};


UCLASS(BlueprintType)
class REPLICATORGENERATOR_API UChannelDataSettingController : public UEditorSubsystem
{
	GENERATED_BODY()

protected:
	TJsonModel<FChannelDataStateSettingRow> ChannelDataStateSettingModal = GenManager_ChannelDataSettingsPath;

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable)
	void GetChannelDataStateOptions(TArray<FChannelDataStateOption>& Options) const;

	UFUNCTION(BlueprintCallable)
	void GetChannelDataSettings(TArray<FChannelDataSetting>& ChannelDataSettings);

	UFUNCTION(BlueprintCallable)
	void SaveChannelDataSettings(const TArray<FChannelDataSetting>& ChannelDataSettings);

	UFUNCTION(BlueprintCallable)
	void ImportChannelDataSettingsFrom(const FString& FilePath, TArray<FChannelDataSetting>& ChannelDataSettings, bool& Success);

	UFUNCTION(BlueprintCallable)
	void ExportChannelDataSettingsTo(const FString& FilePath, const TArray<FChannelDataSetting>& ChannelDataSettings, bool& Success);

	inline TArray<FChannelDataStateSettingRow> ConvertChannelDataSettingsToRows(const TArray<FChannelDataSetting>& ChannelDataSettings);

	inline TArray<FChannelDataSetting> ConvertRowsToChannelDataSettings(const TArray<FChannelDataStateSettingRow>& ChannelDataSettingRows);

	inline void SortChannelDataSettings(TArray<FChannelDataSetting>& ChannelDataSettings);

protected:
	void GetDefaultChannelDataSettings(TArray<FChannelDataSetting>& ChannelDataSettings);
};
