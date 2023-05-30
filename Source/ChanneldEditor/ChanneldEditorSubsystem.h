#pragma once

#include "CoreMinimal.h"
#include "ChanneldMissionNotiProxy.h"
#include "CloudDeploymentController.h"
#include "ChanneldEditorSubsystem.generated.h"

UENUM(BlueprintType)
enum class EUpdateRepActorCacheResult : uint8
{
	URRT_Updated,
	URRT_Updating,
	URRT_Editing,
	URRT_Failed,
};

USTRUCT()
struct CHANNELDEDITOR_API FDockerConfigAuth
{
	GENERATED_BODY()

	UPROPERTY()
	FString Username;

	UPROPERTY()
	FString Password;

	FDockerConfigAuth()
	{
	}
};

DECLARE_DYNAMIC_DELEGATE_OneParam(FPostRepActorCache, EUpdateRepActorCacheResult, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPostGenRepCode, bool, Result);

DECLARE_DYNAMIC_DELEGATE_OneParam(FPostPackageProject, bool, Success);

DECLARE_DYNAMIC_DELEGATE_OneParam(FPostBuildServerDockerImage, bool, Success);

DECLARE_DYNAMIC_DELEGATE_OneParam(FPostBuildChanneldDockerImage, bool, Success);

DECLARE_DYNAMIC_DELEGATE_OneParam(FPostUploadDockerImage, bool, Success);

DECLARE_DYNAMIC_DELEGATE_OneParam(FPostDeploymentToCluster, bool, Success);

DECLARE_DYNAMIC_DELEGATE_OneParam(FPostStopDeployment, bool, Success);

DECLARE_DYNAMIC_DELEGATE_OneParam(FPostCreateK8sSecret, bool, Success);

UCLASS(Meta = (DisplayName = "Channeld Editor"))
class CHANNELDEDITOR_API UChanneldEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FPostGenRepCode PostGenerateReplicationCode;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable)
	void UpdateReplicationCacheAction(FPostRepActorCache PostUpdatedRepActorCache);

	void UpdateReplicationCache(TFunction<void(EUpdateRepActorCacheResult Result)> PostUpdateRegActorCache,
	                            FMissionCanceled* CanceledDelegate);

	UFUNCTION(BlueprintCallable)
	void ChooseFile(FString& FilePath, bool& Success, const FString& DialogTitle, const FString& DefaultPath,
	                const FString& FileTypes = TEXT("All files (*.*)|*.*"));

	UFUNCTION(BlueprintCallable)
	void ChooseFilePathToSave(FString& FilePath, bool& Success, const FString& DialogTitle, const FString& DefaultPath,
	                          const FString& FileTypes = TEXT("All files (*.*)|*.*"));

	UFUNCTION(BlueprintCallable)
	bool NeedToGenerateReplicationCode(bool ShowDialog = false);

	UFUNCTION(BlueprintCallable)
	void GenerateReplicationAction();

	/**
	 * Using the protoc to generate c++ code in the project.
	 */
	void GenRepProtoCppCode(const TArray<FString>& ProtoFiles,
	                        TFunction<void()> PostGenRepProtoCppCodeSuccess = nullptr);

	/**
	 * Using the protoc to generate go code in the channeld.
	 * The channeld directory is read from environment variable 'CHANNELD_PATH'.
	 */
	void GenRepProtoGoCode(const TArray<FString>& ProtoFiles, TFunction<void()> PostGenRepProtoGoCodeSuccess = nullptr);

	void FailedToGenRepCode();

	/**
	 * Recompile the game code. Copied from FLevelEditorActionCallbacks::RecompileGameCode_Clicked().
	 */
	void RecompileGameCode() const;

	UFUNCTION(BlueprintCallable)
	void OpenMessageDialog(FText Message, FText OptTitle);

	UFUNCTION(BlueprintCallable)
	bool CheckDockerCommand();

	UFUNCTION(BlueprintCallable)
	FString GetCloudDepymentProjectIntermediateDir() const;

	UFUNCTION(BlueprintCallable)
	void BuildServerDockerImage(const FString& Tag, const FPostBuildServerDockerImage& PostBuildServerDockerImage);

	UFUNCTION(BlueprintCallable)
	void BuildChanneldDockerImage(const FString& Tag,
	                              const FPostBuildChanneldDockerImage& PostBuildChanneldDockerImage);

	UFUNCTION(BlueprintCallable)
	void OpenPackagingSettings();

	UFUNCTION(BlueprintCallable)
	TMap<FString, FString> GetDockerImageId(const TArray<FString>& Tags);

	UFUNCTION(BlueprintCallable)
	void PackageProject(const FName InPlatformInfoName, const FPostPackageProject& PostPackageProject);

	void AddMessageLog(const FText& Text, const FText& Detail, const FString& TutorialLink,
	                   const FString& DocumentationLink);

	UFUNCTION(BlueprintCallable)
	void UploadDockerImage(const FString& ChanneldImageTag, const FString& ServerImageTag, FString RegistryUsername,
	                       FString RegistryPassword, const FPostUploadDockerImage& PostUploadDockerImage);

	UFUNCTION(BlueprintCallable)
	void DeployToCluster(const FDeploymentStepParams DeploymentParams,
	                     const FPostDeploymentToCluster& PostDeplymentToCluster);

	FString GetCheckPodCommand(const FString& JQPath, const FString& PodStatusJSONPath, const FString& PodSelector,
	                           int32 PodReplicas, const FString& DescriptionName) const;

	UFUNCTION(BlueprintCallable)
	void GetCurrentContext(FString& CurrentContext, bool& Success, FString& Message);

	UFUNCTION(BlueprintCallable)
	void GetCurrentContextNamespace(FString& Namespace, bool& Success, FString& Message);

	UFUNCTION(BlueprintCallable)
	void StopDeployment(FPostStopDeployment PostStopDeployment);

	UFUNCTION(BlueprintCallable)
	void GetDeployedGrafanaURL(FString& URL, bool& Success, FString& Message);

	UFUNCTION(BlueprintCallable)
	void GetChanneldExternalIP(FString& ExternalIP, bool& Success, FString& Message);

	UFUNCTION(BlueprintCallable)
	void CreateK8sSecret(const FString& Name, const FString& DockerImage, const FString& TargetClusterContext,
	                     const FString& Namespace,
	                     const FString& Username, const FString& Password, FPostCreateK8sSecret PostCreateK8sSecret);

	UFUNCTION(BlueprintCallable)
	void CopyMessageToClipboard(const FString& Message);

	UFUNCTION(BlueprintCallable)
	bool IsValidAssetPath(const FString& Path);

private:
	TSharedPtr<FChanneldProcWorkerThread> UpdateRepActorCacheWorkThread;
	UChanneldMissionNotiProxy* UpdateRepActorCacheNotify;
	bool bUpdatingRepActorCache;

	TSharedPtr<FChanneldProcWorkerThread> GenRepWorkThread;
	UChanneldMissionNotiProxy* GenRepNotify;
	bool bGeneratingReplication;

	TSharedPtr<FChanneldProcWorkerThread> GenProtoCppCodeWorkThread;
	TSharedPtr<FChanneldProcWorkerThread> GenProtoGoCodeWorkThread;

	UChanneldMissionNotiProxy* BuildServerDockerImageNotify;
	UChanneldMissionNotiProxy* BuildChanneldDockerImageNotify;

	UChanneldMissionNotiProxy* UploadDockerImageNotify;

	TSharedPtr<FChanneldProcWorkerThread> DeployToClusterWorkThread;
	UChanneldMissionNotiProxy* DeployToClusterNotify;

	TSharedPtr<FChanneldProcWorkerThread> StopDeploymentWorkThread;
	UChanneldMissionNotiProxy* StopDeploymentNotify;

	TSharedPtr<FChanneldProcWorkerThread> CreateK8sSecretWorkThread;
	UChanneldMissionNotiProxy* CreateK8sSecretNotify;
};
