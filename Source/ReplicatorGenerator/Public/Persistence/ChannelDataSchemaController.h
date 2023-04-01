#pragma once

#include "CoreMinimal.h"
#include "ChanneldTypes.h"
#include "JsonModel.h"
#include "ReplicatorGeneratorDefinition.h"
#include "ChannelDataSchemaController.generated.h"

USTRUCT(BlueprintType)
struct REPLICATORGENERATOR_API FChannelDataStateSchemaRow : public FTableRowBase
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

	FChannelDataStateSchemaRow() = default;

	FChannelDataStateSchemaRow(int32 InChannelType, int32 InChannelTypeOrder, const FString& InReplicationClassPath, int32 InStateOrder, bool InbSkip, bool InbSingleton)
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
struct REPLICATORGENERATOR_API FChannelDataStateSchema
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

	FChannelDataStateSchema() = default;

	FChannelDataStateSchema(EChanneldChannelType InChannelType, const FString& InReplicationClassPath, int32 InStateOrder)
		: ChannelType(InChannelType)
		  , ReplicationClassPath(InReplicationClassPath)
		  , StateOrder(InStateOrder)
	{
	}

	FChannelDataStateSchema(EChanneldChannelType InChannelType, const FString& InReplicationClassPath, int32 InStateOrder, bool InbSkip, bool InbSingleton)
		: ChannelType(InChannelType)
		  , ReplicationClassPath(InReplicationClassPath)
		  , StateOrder(InStateOrder)
		  , bSkip(InbSkip)
		  , bSingleton(InbSingleton)
	{
	}
};

USTRUCT(BlueprintType)
struct REPLICATORGENERATOR_API FChannelDataSchema
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EChanneldChannelType ChannelType = EChanneldChannelType::ECT_Unknown;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 ChannelTypeOrder = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FChannelDataStateSchema> StateSchemata;

	FChannelDataSchema() = default;

	FChannelDataSchema(EChanneldChannelType InChannelType, int32 InChannelTypeOrder)
		: ChannelType(InChannelType)
		  , ChannelTypeOrder(InChannelTypeOrder)
	{
	}

	FChannelDataSchema(int32 InChannelType, int32 InChannelTypeOrder)
		: ChannelType(static_cast<EChanneldChannelType>(InChannelType))
		  , ChannelTypeOrder(InChannelTypeOrder)
	{
	}

	void Sort()
	{
		StateSchemata.Sort([](const FChannelDataStateSchema& A, const FChannelDataStateSchema& B)
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
class REPLICATORGENERATOR_API UChannelDataSchemaController : public UEditorSubsystem
{
	GENERATED_BODY()

protected:
	TJsonModel<FChannelDataStateSchemaRow> ChannelDataStateSchemaModal = GenManager_ChannelDataSchemataPath;

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable)
	void GetChannelDataStateOptions(TArray<FChannelDataStateOption>& Options) const;

	UFUNCTION(BlueprintCallable)
	void GetChannelDataSchemata(TArray<FChannelDataSchema>& ChannelDataSchemata);

	UFUNCTION(BlueprintCallable)
	void SaveChannelDataSchemata(const TArray<FChannelDataSchema>& ChannelDataSchemata);

	UFUNCTION(BlueprintCallable)
	void ImportChannelDataSchemataFrom(const FString& FilePath, TArray<FChannelDataSchema>& ChannelDataSchemata, bool& Success);

	UFUNCTION(BlueprintCallable)
	void ExportChannelDataSchemataTo(const FString& FilePath, const TArray<FChannelDataSchema>& ChannelDataSchemata, bool& Success);

	inline TArray<FChannelDataStateSchemaRow> ConvertChannelDataSchemataToRows(const TArray<FChannelDataSchema>& ChannelDataSchemata);

	inline TArray<FChannelDataSchema> ConvertRowsToChannelDataSchemata(const TArray<FChannelDataStateSchemaRow>& ChannelDataSchemaRows);

	inline void SortChannelDataSchemata(TArray<FChannelDataSchema>& ChannelDataSchemata);

protected:
	void GetDefaultChannelDataSchemata(TArray<FChannelDataSchema>& ChannelDataSchemata);
};
