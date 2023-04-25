#pragma once

#include "CoreMinimal.h"
#include "ChanneldMissionNotiProxy.h"
#include "ChanneldEditorSubsystem.generated.h"

UENUM(BlueprintType)
enum class EUpdateRepActorCacheResult : uint8
{
	URRT_Updated,
	URRT_Updating,
	URRT_Editing,
	URRT_Failed,
};

DECLARE_DYNAMIC_DELEGATE_OneParam(FPostRepActorCache, EUpdateRepActorCacheResult, Result);

UCLASS(Meta = (DisplayName = "Channeld Editor"))
class CHANNELDEDITOR_API UChanneldEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable)
	void UpdateReplicationCacheAction(FPostRepActorCache PostUpdatedRepActorCache);

	void UpdateReplicationCache(TFunction<void(EUpdateRepActorCacheResult Result)> PostUpdateRegActorCache, FMissionCanceled* CanceledDelegate);

	UFUNCTION(BlueprintCallable)
	void ChooseFile(FString& FilePath, bool& Success, const FString& DialogTitle, const FString& DefaultPath, const FString& FileTypes = TEXT("All files (*.*)|*.*"));

	UFUNCTION(BlueprintCallable)
	void ChooseFilePathToSave(FString& FilePath, bool& Success, const FString& DialogTitle, const FString& DefaultPath, const FString& FileTypes = TEXT("All files (*.*)|*.*"));

	UFUNCTION(BlueprintCallable)
	void GenerateReplicationAction();
	
	/**
	 * Using the protoc to generate c++ code in the project.
	 */
	void GenRepProtoCppCode(const TArray<FString>& ProtoFiles, TFunction<void()> PostGenRepProtoCppCodeSuccess = nullptr);

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

private:
	TSharedPtr<FChanneldProcWorkerThread> UpdateRepActorCacheWorkThread;
	UChanneldMissionNotiProxy* UpdateRepActorCacheNotify;
	bool bUpdatingRepActorCache;

	TSharedPtr<FChanneldProcWorkerThread> GenRepWorkThread;
	UChanneldMissionNotiProxy* GenRepNotify;
	bool bGeneratingReplication;

	TSharedPtr<FChanneldProcWorkerThread> GenProtoCppCodeWorkThread;
	TSharedPtr<FChanneldProcWorkerThread> GenProtoGoCodeWorkThread;
};
