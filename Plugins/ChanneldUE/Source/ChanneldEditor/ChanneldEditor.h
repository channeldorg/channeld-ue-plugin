#pragma once

#include "CoreMinimal.h"
#include "ChanneldEditorSettings.h"

DECLARE_LOG_CATEGORY_CLASS(LogChanneldEditor, Log, All);

class FChanneldProcWorkerThread;
class UChanneldMissionNotiProxy;
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
	void AddMenuEntry(FMenuBuilder& Builder);
	void FillSubmenu(FMenuBuilder& Builder);

	void LaunchChanneldAction();
	void StopChanneldAction();
	FTimerManager* GetTimerManager();
	void LaunchServersAction();
	void StopServersAction();

	void GenerateReplicatorAction();
	void AddRepCompsToBPsAction();

	void RecompileGameCode() const;

	void GenReplicatorProto();

	void OpenEditorSettingsAction();
	
	void LaunchServerGroup(const FServerGroup& ServerGroup);

	FProcHandle ChanneldProcHandle;
	
	TSharedPtr<class FUICommandList> PluginCommands;
	TSharedRef<SWidget> CreateMenuContent(TSharedPtr<FUICommandList> Commands);
	TArray<FProcHandle> ServerProcHandles;

	mutable TSharedPtr<FChanneldProcWorkerThread> GenRepWorkThread;
	mutable TSharedPtr<FChanneldProcWorkerThread> GenProtoWorkThread;
	UChanneldMissionNotiProxy* GenRepMissionNotifyProxy;
	UChanneldMissionNotiProxy* GenProtoMissionNotifyProxy;

	mutable TSharedPtr<FChanneldProcWorkerThread> AddRepCompWorkThread;
	UChanneldMissionNotiProxy* AddRepCompMissionNotifyProxy;

};
