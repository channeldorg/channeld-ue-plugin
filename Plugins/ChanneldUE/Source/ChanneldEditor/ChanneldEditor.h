#pragma once

#include "CoreMinimal.h"
#include "ChanneldEditorSettings.h"
#include "ReplicatorGeneratorManager.h"

DECLARE_LOG_CATEGORY_CLASS(LogChanneldEditor, Log, All);

class FChanneldProcWorkerThread;
class UChanneldMissionNotiProxy;
class UChanneldGetawayNotiProxy;
class FToolBarBuilder;
class FMenuBuilder;
class SWidget;
class FUICommandList;

enum EChanneldLaunchResult
{
	Launched,
	AlreadyLaunched,
	Building,
	Failed,
};

class FChanneldEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void AddToolbarButton(FToolBarBuilder& Builder);

	static bool IsNetworkingEnabled();
	static void ToggleNetworkingAction();
	void LaunchChanneldAction();
	void LaunchChanneldAction(TFunction<void(EChanneldLaunchResult Result)> PostChanneldLaunched = nullptr);
	void StopChanneldAction();
	FTimerManager* GetTimerManager();
	void LaunchServersAction();
	void StopServersAction();
	void LaunchChanneldAndServersAction();

	void GenerateReplicatorAction();
	void AddRepCompsToBPsAction();

	/**
	 * Using the protoc to generate c++ code in the project.
	 */
	void GenRepProtoCppCode(const TArray<FString>& ProtoFiles) const;

	/**
	 * Using the protoc to generate go code in the channeld.
	 * The channeld directory is read from environment variable 'CHANNELD_PATH'.
	 */
	void GenRepProtoGoCode(const TArray<FString>& ProtoFiles, const FGeneratedManifest& Manifest, const FString& ReplicatorStorageDir) const;

	/**
	 * Recompile the game code. Copied from FLevelEditorActionCallbacks::RecompileGameCode_Clicked().
	 */
	void RecompileGameCode() const;

	void OpenEditorSettingsAction();
	
	void LaunchServerGroup(const FServerGroup& ServerGroup);

	FProcHandle ChanneldProcHandle;
	
	TSharedPtr<class FUICommandList> PluginCommands;
	TSharedRef<SWidget> CreateMenuContent(TSharedPtr<FUICommandList> Commands);
	TArray<FProcHandle> ServerProcHandles;

	mutable TSharedPtr<FChanneldProcWorkerThread> BuildChanneldWorkThread;
	UChanneldMissionNotiProxy* BuildChanneldNotify;

	mutable TSharedPtr<FChanneldProcWorkerThread> GenRepWorkThread;
	mutable TSharedPtr<FChanneldProcWorkerThread> GenProtoCppCodeWorkThread;
	mutable TSharedPtr<FChanneldProcWorkerThread> GenProtoGoCodeWorkThread;
	UChanneldMissionNotiProxy* GenRepNotify;

	mutable TSharedPtr<FChanneldProcWorkerThread> AddRepCompWorkThread;
	UChanneldMissionNotiProxy* AddRepCompNotify;

};
