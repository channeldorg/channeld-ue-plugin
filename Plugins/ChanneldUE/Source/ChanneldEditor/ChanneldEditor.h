#pragma once

#include "CoreMinimal.h"

class FChanneldProcWorkerThread;
class UChanneldMissionNotiProxy;
class FToolBarBuilder;
class FMenuBuilder;
class SWidget;
class FUICommandList;

DECLARE_LOG_CATEGORY_EXTERN(LogChanneldEditor, Log, All);

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
	void LaunchServersAction();
	void StopServersAction();

	void GenerateReplicatorAction();
	void AddRepCompToBPAction();

	void GenReplicatorProto(FChanneldProcWorkerThread* ProcWorker);

	TSharedPtr<class FUICommandList> PluginCommands;
	TSharedRef<SWidget> CreateMenuContent(TSharedPtr<FUICommandList> Commands);
	TArray<FProcHandle> ServerProcHandles;

	mutable TSharedPtr<FChanneldProcWorkerThread> GenRepWorkThread;
	mutable TSharedPtr<FChanneldProcWorkerThread> GenProtoWorkThread;
	UChanneldMissionNotiProxy* GenRepMissionNotifyProxy;

	mutable TSharedPtr<FChanneldProcWorkerThread> AddRepCompWorkThread;
	UChanneldMissionNotiProxy* AddRepCompMissionNotifyProxy;

};
