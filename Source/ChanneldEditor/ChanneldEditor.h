#pragma once

#include "CoreMinimal.h"
#include "ChanneldEditorSettings.h"
#include "ChanneldMissionNotiProxy.h"
#include "ReplicatorGeneratorManager.h"

class FChanneldProcWorkerThread;
class UChanneldMissionNotiProxy;
class UChanneldGetawayNotiProxy;
class FToolBarBuilder;
class FMenuBuilder;
class SWidget;
class FUICommandList;

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
	void LaunchChanneldAction(TFunction<void(bool IsLaunched)> PostChanneldLaunched = nullptr);
	void StopChanneldAction();
	FTimerManager* GetTimerManager();
	void LaunchServersAction();
	void StopServersAction();
	void LaunchChanneldAndServersAction();
	void LaunchServerGroup(const FServerLaunchGroup& ServerGroup);

	static bool IsCompatibleRecompilationEnabled();
	static void ToggleCompatibleRecompilationAction();

	void GenerateReplicationAction();
	void OpenEditorSettingsAction();

	void AddRepCompsToBPsAction();

	void OpenChannelDataEditorAction();
	void OpenCloudDeploymentAction();
	
	void RemoveDuplicateWorldSettingsAction();

	FProcHandle ChanneldProcHandle;

	TSharedPtr<class FUICommandList> PluginCommands;
	TSharedRef<SWidget> CreateMenuContent(TSharedPtr<FUICommandList> Commands);
	TArray<FTimerHandle> ServerGroupDelayHandles;

	TArray<FProcHandle> ServerProcHandles;

	mutable TSharedPtr<FChanneldProcWorkerThread> BuildChanneldWorkThread;
	UChanneldMissionNotiProxy* BuildChanneldNotify;

	mutable TSharedPtr<FChanneldProcWorkerThread> AddRepCompWorkThread;
	UChanneldMissionNotiProxy* AddRepCompNotify;
};
