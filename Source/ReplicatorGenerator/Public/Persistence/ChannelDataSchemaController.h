#pragma once

#include "CoreMinimal.h"
#include "ChanneldTypes.h"
#include "JsonModel.h"
#include "ReplicatorGeneratorDefinition.h"
#include "ChannelDataSchemaController.generated.h"

USTRUCT(BlueprintType)
struct REPLICATORGENERATOR_API FChannelDataStateSchema
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	FString ReplicationClassPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 StateOrder = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bSkip = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bSingleton = false;

	FChannelDataStateSchema() = default;

	FChannelDataStateSchema(const FString& InReplicationClassPath, int32 InStateOrder)
		: ReplicationClassPath(InReplicationClassPath)
		  , StateOrder(InStateOrder)
	{
	}

	FChannelDataStateSchema(const FString& InReplicationClassPath, int32 InStateOrder, bool InbSkip, bool InbSingleton)
		: ReplicationClassPath(InReplicationClassPath)
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

DECLARE_DELEGATE(FPostDataSchemaUndoRedoDelegate)

UCLASS()
class UChannelDataSchemaTransaction : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<FChannelDataSchema> ChannelDataSchemata;

	virtual void PostEditUndo() override;

	FPostDataSchemaUndoRedoDelegate PostDataSchemaUndoRedo;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPostDataSchemaUpdateDelegate, const TArray<FChannelDataSchema>&, DataSchemata);

UCLASS(BlueprintType)
class REPLICATORGENERATOR_API UChannelDataSchemaController : public UEditorSubsystem
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	UChannelDataSchemaTransaction* ChannelDataSchemaTransaction;
	
	TJsonModel<FChannelDataSchema> ChannelDataSchemaModel = GenManager_ChannelDataSchemataPath;
	TJsonModel<FChannelDataSchema> DefaultChannelDataSchemaModel;

public:
	UPROPERTY(BlueprintAssignable)
	FPostDataSchemaUpdateDelegate PostDataSchemaUpdated;
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable)
	void GetUnhiddenChannelTypes(TArray<EChanneldChannelType>& ChannelTypes) const;

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

	UFUNCTION(BlueprintCallable)
	void GetDefaultChannelDataSchemata(TArray<FChannelDataSchema>& ChannelDataSchemata);

	UFUNCTION(BlueprintCallable)
	FDateTime GetLastUpdatedTime() const;

protected:
	inline void SortChannelDataSchemata(TArray<FChannelDataSchema>& ChannelDataSchemata);

	void HandlePostDataSchemaUndoRedo();
};
