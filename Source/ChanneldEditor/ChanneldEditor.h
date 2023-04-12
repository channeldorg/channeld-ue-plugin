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

enum EChanneldLaunchResult : uint8
{
	CLR_Launched,
	CLR_AlreadyLaunched,
	CLR_Building,
	CLR_Failed,
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
	void LaunchServerGroup(const FServerGroup& ServerGroup);

	static bool IsCompatibleRecompilationEnabled();
	static void ToggleCompatibleRecompilationAction();

	void GenerateReplicationAction();
	void OpenEditorSettingsAction();

	void AddRepCompsToBPsAction();

	void OpenChannelDataEditorAction();

	FProcHandle ChanneldProcHandle;

	TSharedPtr<class FUICommandList> PluginCommands;
	TSharedRef<SWidget> CreateMenuContent(TSharedPtr<FUICommandList> Commands);
	TArray<FProcHandle> ServerProcHandles;

	mutable TSharedPtr<FChanneldProcWorkerThread> BuildChanneldWorkThread;
	UChanneldMissionNotiProxy* BuildChanneldNotify;

	mutable TSharedPtr<FChanneldProcWorkerThread> AddRepCompWorkThread;
	UChanneldMissionNotiProxy* AddRepCompNotify;
};
