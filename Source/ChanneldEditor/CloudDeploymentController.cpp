// Fill out your copyright notice in the Description page of Project Settings.


#include "CloudDeploymentController.h"
#include "ChanneldEditorTypes.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"

void UCloudDeploymentController::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

FCloudDeploymentParams UCloudDeploymentController::LoadCloudDeploymentParams()
{
	FCloudDeploymentParams Result;
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *JsonPath))
	{
		return Result;
	}
	if (!FJsonObjectConverter::JsonObjectStringToUStruct<FCloudDeploymentParams>(JsonString, &Result, 0, 0))
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Failed to parse json from file: %s"), *JsonPath);
		return Result;
	}
	return Result;
}

void UCloudDeploymentController::SaveCloudDeploymentParams(const FCloudDeploymentParams& InParams)
{
	FString JsonString;
	if (!FJsonObjectConverter::UStructToJsonObjectString(InParams, JsonString, 0, 0))
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Failed to convert data to json: %s"), *JsonPath);
	}
	if (!FFileHelper::SaveStringToFile(JsonString, *JsonPath))
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Failed to save data to file: %s"), *JsonPath);
	}
}

FPackageStepParams UCloudDeploymentController::LoadPackageStepParams()
{
	return LoadCloudDeploymentParams().PackageStepParams;
}

void UCloudDeploymentController::SavePackageStepParams(const FPackageStepParams& InParams)
{
	FCloudDeploymentParams CloudDeploymentParams = LoadCloudDeploymentParams();
	CloudDeploymentParams.PackageStepParams = InParams;
	SaveCloudDeploymentParams(CloudDeploymentParams);
}

FUploadStepParams UCloudDeploymentController::LoadUploadStepParams()
{
	return LoadCloudDeploymentParams().UploadStepParams;
}

void UCloudDeploymentController::SaveUploadStepParams(const FUploadStepParams& InParams)
{
	FCloudDeploymentParams CloudDeploymentParams = LoadCloudDeploymentParams();
	CloudDeploymentParams.UploadStepParams = InParams;
	SaveCloudDeploymentParams(CloudDeploymentParams);
}

FDeploymentStepParams UCloudDeploymentController::LoadDeploymentStepParams()
{
	return LoadCloudDeploymentParams().DeploymentStepParams;
}

void UCloudDeploymentController::SaveDeploymentStepParams(const FDeploymentStepParams& InParams)
{
	FCloudDeploymentParams CloudDeploymentParams = LoadCloudDeploymentParams();
	CloudDeploymentParams.DeploymentStepParams = InParams;
	SaveCloudDeploymentParams(CloudDeploymentParams);
}
