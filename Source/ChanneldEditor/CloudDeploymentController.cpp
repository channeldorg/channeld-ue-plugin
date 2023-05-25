// Fill out your copyright notice in the Description page of Project Settings.


#include "CloudDeploymentController.h"

#include "ChanneldEditorSubsystem.h"
#include "ChanneldEditorTypes.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"

void UCloudDeploymentController::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	DeploymentParamJsonPath = GEditor->GetEditorSubsystem<UChanneldEditorSubsystem>()->GetCloudDepymentProjectIntermediateDir() / TEXT("CloudDeploymentParam.json");
	OneClickDeploymentResultJsonPath = GEditor->GetEditorSubsystem<UChanneldEditorSubsystem>()->GetCloudDepymentProjectIntermediateDir() / TEXT("OneClickDeploymentResult.json");
}

FCloudDeploymentParams UCloudDeploymentController::LoadCloudDeploymentParams()
{
	FCloudDeploymentParams Result;
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *DeploymentParamJsonPath))
	{
		return Result;
	}
	if (!FJsonObjectConverter::JsonObjectStringToUStruct<FCloudDeploymentParams>(JsonString, &Result, 0, 0))
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Failed to parse json from file: %s"), *DeploymentParamJsonPath);
		return Result;
	}
	return Result;
}

void UCloudDeploymentController::SaveCloudDeploymentParams(const FCloudDeploymentParams& InParams)
{
	FString JsonString;
	if (!FJsonObjectConverter::UStructToJsonObjectString(InParams, JsonString, 0, 0))
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Failed to convert data to json: %s"), *DeploymentParamJsonPath);
	}
	if (!FFileHelper::SaveStringToFile(JsonString, *DeploymentParamJsonPath))
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Failed to save data to file: %s"), *DeploymentParamJsonPath);
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
	if(!FPaths::FileExists(DeploymentParamJsonPath))
	{
		return GetDefaultDeploymentStepParams();
	}
	return LoadCloudDeploymentParams().DeploymentStepParams;
}

FDeploymentStepParams UCloudDeploymentController::GetDefaultDeploymentStepParams()
{
	FDeploymentStepParams DefaultParams;
	DefaultParams.YAMLTemplatePath = FString(ANSI_TO_TCHAR(PLUGIN_DIR)) / TEXT("Template") / TEXT(
		"Deployment.yaml");
	auto EditorSetting = GetMutableDefault<UChanneldEditorSettings>();
	DefaultParams.ChanneldParams = EditorSetting->LaunchChanneldParameters;
	TArray<FServerGroup> EditorServerGroup = EditorSetting->ServerGroups;
	for (int32 I = 0; I < EditorServerGroup.Num(); ++I)
	{
		FServerGroup ServerGroup = EditorServerGroup[I];
		FServerGroupForDeployment ServerGroupForDeployment;
		ServerGroupForDeployment.bEnabled = ServerGroup.bEnabled;
		ServerGroupForDeployment.ServerNum = ServerGroup.ServerNum;
		ServerGroupForDeployment.ServerMap = ServerGroup.ServerMap.IsValid() ? ServerGroup.ServerMap : GEditor->GetEditorWorldContext().World()->GetOuter()->GetName();
		ServerGroupForDeployment.ServerViewClass = ServerGroup.ServerViewClass;
		ServerGroupForDeployment.AdditionalArgs = ServerGroup.AdditionalArgs;
		if (I == 0)
		{
			ServerGroupForDeployment.YAMLTemplatePath = FString(ANSI_TO_TCHAR(PLUGIN_DIR)) / TEXT("Template") / TEXT(
				"ServerDeployment.yaml");
		}
		else if (I == 1)
		{
			ServerGroupForDeployment.YAMLTemplatePath = FString(ANSI_TO_TCHAR(PLUGIN_DIR)) / TEXT("Template") / TEXT(
				"SpatialServerDeployment.yaml");
		}
		DefaultParams.ServerGroups.Add(ServerGroupForDeployment);
	}
	return DefaultParams;
}

void UCloudDeploymentController::SaveDeploymentStepParams(const FDeploymentStepParams& InParams)
{
	FCloudDeploymentParams CloudDeploymentParams = LoadCloudDeploymentParams();
	CloudDeploymentParams.DeploymentStepParams = InParams;
	SaveCloudDeploymentParams(CloudDeploymentParams);
}

FOneClickDeploymentResult UCloudDeploymentController::LoadOneClickDeploymentResult()
{
	FOneClickDeploymentResult Result;
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *OneClickDeploymentResultJsonPath))
	{
		return Result;
	}
	if (!FJsonObjectConverter::JsonObjectStringToUStruct<FOneClickDeploymentResult>(JsonString, &Result, 0, 0))
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Failed to parse json from file: %s"), *OneClickDeploymentResultJsonPath);
		return Result;
	}
	return Result;
}

void UCloudDeploymentController::SaveOneClickDeploymentPackageResult()
{
	const FPackageStepParams& PackageStepParams = LoadPackageStepParams();
	auto ImageIds = GEditor->GetEditorSubsystem<UChanneldEditorSubsystem>()->GetDockerImageId({
		PackageStepParams.ChanneldImageTag, PackageStepParams.ServerImageTag
	});
	FOneClickDeploymentResult Result;
	Result.BuiltChanneldImageTag = PackageStepParams.ChanneldImageTag;
	Result.BuiltChanneldImageId = ImageIds[PackageStepParams.ChanneldImageTag];
	Result.BuiltServerImageTag = PackageStepParams.ServerImageTag;
	Result.BuiltServerImageId = ImageIds[PackageStepParams.ServerImageTag];

	FString JsonString;
	if (!FJsonObjectConverter::UStructToJsonObjectString(Result, JsonString, 0, 0))
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Failed to convert data to json: %s"), *OneClickDeploymentResultJsonPath);
	}
	if (!FFileHelper::SaveStringToFile(JsonString, *OneClickDeploymentResultJsonPath))
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Failed to save data to file: %s"), *OneClickDeploymentResultJsonPath);
	}
}

bool UCloudDeploymentController::CheckOneClickDeploymentBuiltImageLatest()
{
	auto PackageStepParams = LoadPackageStepParams();
	auto OneClickDeploymentResult = LoadOneClickDeploymentResult();
	if (
		OneClickDeploymentResult.BuiltChanneldImageTag.IsEmpty() || OneClickDeploymentResult.BuiltServerImageTag.
		IsEmpty()
		|| PackageStepParams.ChanneldImageTag.IsEmpty() || PackageStepParams.ServerImageTag.IsEmpty()
		|| PackageStepParams.ChanneldImageTag != OneClickDeploymentResult.BuiltChanneldImageTag
		|| PackageStepParams.ServerImageTag != OneClickDeploymentResult.BuiltServerImageTag
	)
	{
		return false;
	}
	auto ImageIds = GEditor->GetEditorSubsystem<UChanneldEditorSubsystem>()->GetDockerImageId({
		PackageStepParams.ChanneldImageTag, PackageStepParams.ServerImageTag
	});
	return ImageIds[OneClickDeploymentResult.BuiltChanneldImageTag] == OneClickDeploymentResult.BuiltChanneldImageId &&
		ImageIds[OneClickDeploymentResult.BuiltServerImageTag] == OneClickDeploymentResult.BuiltServerImageId;
}

void UCloudDeploymentController::SaveOneClickDeploymentUploadResult()
{
	FOneClickDeploymentResult Result = LoadOneClickDeploymentResult();
	Result.UploadedChanneldImageTag = Result.BuiltChanneldImageTag;
	Result.UploadedChanneldImageId = Result.BuiltChanneldImageId;
	Result.UploadedServerImageTag = Result.BuiltServerImageTag;
	Result.UploadedServerImageId = Result.BuiltServerImageId;

	FString JsonString;
	if (!FJsonObjectConverter::UStructToJsonObjectString(Result, JsonString, 0, 0))
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Failed to convert data to json: %s"), *OneClickDeploymentResultJsonPath);
	}
	if (!FFileHelper::SaveStringToFile(JsonString, *OneClickDeploymentResultJsonPath))
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Failed to save data to file: %s"), *OneClickDeploymentResultJsonPath);
	}
}

void UCloudDeploymentController::SaveOneClickDeploymentDeploymentResult()
{
	const FDeploymentStepParams& DeploymentStepParams = LoadDeploymentStepParams();
	FOneClickDeploymentResult Result = LoadOneClickDeploymentResult();
	Result.DeployedChanneldImageTag = Result.UploadedChanneldImageTag;
	Result.DeployedChanneldImageId = Result.UploadedChanneldImageId;
	Result.DeployedServerImageTag = Result.UploadedServerImageTag;
	Result.DeployedServerImageId = Result.UploadedServerImageId;
	Result.Cluster = DeploymentStepParams.Cluster;
	Result.Namespace = DeploymentStepParams.Namespace;
	Result.ImagePullSecret = DeploymentStepParams.ImagePullSecret;
	Result.YAMLTemplatePath = DeploymentStepParams.YAMLTemplatePath;
	Result.ChanneldParams = DeploymentStepParams.ChanneldParams;
	Result.ServerGroups = DeploymentStepParams.ServerGroups;

	FString JsonString;
	if (!FJsonObjectConverter::UStructToJsonObjectString(Result, JsonString, 0, 0))
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Failed to convert data to json: %s"), *OneClickDeploymentResultJsonPath);
	}
	if (!FFileHelper::SaveStringToFile(JsonString, *OneClickDeploymentResultJsonPath))
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Failed to save data to file: %s"), *OneClickDeploymentResultJsonPath);
	}
}
