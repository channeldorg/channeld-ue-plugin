// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChanneldEditorSettings.h"
#include "EditorSubsystem.h"
#include "CloudDeploymentController.generated.h"

USTRUCT(BlueprintType)
struct CHANNELDEDITOR_API FServerGroupForDeployment : public FServerGroup
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString YAMLTemplatePath;

	FServerGroupForDeployment()
	{
	}
};

USTRUCT(BlueprintType)
struct CHANNELDEDITOR_API FPackageStepParams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString ChanneldImageTag;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString ServerImageTag;

	FPackageStepParams()
	{
	}
};

USTRUCT(BlueprintType)
struct CHANNELDEDITOR_API FUploadStepParams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString ChanneldImageTag;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString ServerImageTag;

	FUploadStepParams()
	{
	}
};

USTRUCT(BlueprintType)
struct CHANNELDEDITOR_API FDeploymentStepParams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString ChanneldImageTag;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString ServerImageTag;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Cluster;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Namespace;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString YAMLTemplatePath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FString> ChanneldParams;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FServerGroupForDeployment> ServerGroups;

	FDeploymentStepParams()
	{
	}
};

USTRUCT(BlueprintType)
struct CHANNELDEDITOR_API FCloudDeploymentParams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FPackageStepParams PackageStepParams;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FUploadStepParams UploadStepParams;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FDeploymentStepParams DeploymentStepParams;

	FCloudDeploymentParams()
	{
	}
};

USTRUCT(BlueprintType)
struct CHANNELDEDITOR_API FOneClickDeploymentResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString BuiltChanneldImageTag;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString BuiltChanneldImageId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString BuiltServerImageTag;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString BuiltServerImageId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString UploadedChanneldImageTag;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString UploadedChanneldImageId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString UploadedServerImageTag;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString UploadedServerImageId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString DeployedChanneldImageTag;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString DeployedChanneldImageId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString DeployedServerImageTag;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString DeployedServerImageId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Cluster;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Namespace;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString YAMLTemplatePath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FString> ChanneldParams;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FServerGroupForDeployment> ServerGroups;
	
	FOneClickDeploymentResult()
	{
	}
};

UCLASS(BlueprintType, Blueprintable)
class CHANNELDEDITOR_API UCloudDeploymentController : public UEditorSubsystem
{
	GENERATED_BODY()

protected:
	FString DeploymentParamJsonPath = FPaths::ProjectIntermediateDir() / TEXT("ChanneldClouldDeployment") / TEXT(
		"CloudDeploymentParam.json");

	FString OneClickDeploymentResultJsonPath = FPaths::ProjectIntermediateDir() / TEXT("ChanneldClouldDeployment") /
		TEXT(
			"OneClickDeploymentResult.json");

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable)
	FCloudDeploymentParams LoadCloudDeploymentParams();

	UFUNCTION(BlueprintCallable)
	void SaveCloudDeploymentParams(const FCloudDeploymentParams& InParams);

	UFUNCTION(BlueprintCallable)
	FPackageStepParams LoadPackageStepParams();

	UFUNCTION(BlueprintCallable)
	void SavePackageStepParams(const FPackageStepParams& InParams);

	UFUNCTION(BlueprintCallable)
	FUploadStepParams LoadUploadStepParams();

	UFUNCTION(BlueprintCallable)
	void SaveUploadStepParams(const FUploadStepParams& InParams);

	UFUNCTION(BlueprintCallable)
	FDeploymentStepParams LoadDeploymentStepParams();

	UFUNCTION(BlueprintCallable)
	FDeploymentStepParams GetDefaultDeploymentStepParams();

	UFUNCTION(BlueprintCallable)
	void SaveDeploymentStepParams(const FDeploymentStepParams& InParams);

	UFUNCTION(BlueprintCallable)
	FOneClickDeploymentResult LoadOneClickDeploymentResult();

	UFUNCTION(BlueprintCallable)
	void SaveOneClickDeploymentPackageResult();

	UFUNCTION(BlueprintCallable)
	bool CheckOneClickDeploymentBuiltImageLatest();

	UFUNCTION(BlueprintCallable)
	void SaveOneClickDeploymentUploadResult();

	UFUNCTION(BlueprintCallable)
	void SaveOneClickDeploymentDeploymentResult();

};
