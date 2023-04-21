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
	void ToggleChanneldAndServersAction();
	void LaunchChanneldAndServersAction();

	void GenerateReplicationAction();
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

	// When building channeld "Toggle Channeld And Servers", the build channeld process will end.
	// Quickly click "Toggle Channeld And Servers" again (interval <0.2s) to execute LaunchChanneldAction.
	// At this time, the build channeld process has ended, and build channeld is required,
	// so BuildChanneldWorkThread will be reassigned. At this time, the "Run()" is still executing in last BuildChanneldWorkThread,
	// and an exception will be thrown after the "Sleep(0.2)" is over because the last BuildChanneldWorkThread has been destructed.
	// So we need to throttle the "ToggleChanneldAndServersAction" to avoid this problem.
	FTimerHandle ToggleChanneldAndServersThrottle;

	FProcHandle ChanneldProcHandle;
	
	TSharedPtr<class FUICommandList> PluginCommands;
	TSharedRef<SWidget> CreateMenuContent(TSharedPtr<FUICommandList> Commands);
	TArray<FProcHandle> ServerProcHandles;

	mutable TSharedPtr<FChanneldProcWorkerThread> BuildChanneldWorkThread;
	UChanneldMissionNotiProxy* BuildChanneldNotify;

	mutable TSharedPtr<FChanneldProcWorkerThread> GenRepWorkThread;
	mutable bool bGeneratingReplication;

	mutable TSharedPtr<FChanneldProcWorkerThread> GenProtoCppCodeWorkThread;
	mutable TSharedPtr<FChanneldProcWorkerThread> GenProtoGoCodeWorkThread;
	UChanneldMissionNotiProxy* GenRepNotify;

	mutable TSharedPtr<FChanneldProcWorkerThread> AddRepCompWorkThread;
	UChanneldMissionNotiProxy* AddRepCompNotify;

};
