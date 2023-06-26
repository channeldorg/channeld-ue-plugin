#include "ChanneldEditor.h"

#include "BlueprintCompilationManager.h"
#include "ChanneldEditorCommands.h"
#include "ChanneldEditorSettings.h"
#include "ChanneldEditorStyle.h"
#include "ChanneldMissionNotiProxy.h"
#include "AddCompToBPSubsystem.h"
#include "ChanneldEditorSubsystem.h"
#include "ChanneldSettings.h"
#include "ChanneldSettingsDetails.h"
#include "LevelEditor.h"
#include "ReplicatorGeneratorManager.h"
#include "ChanneldUE/Replication/ChanneldReplicationComponent.h"
#include "Widgets/Input/SSpinBox.h"
#include "ThreadUtils/FChanneldProcWorkerThread.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyEditorDelegates.h"
#include "ChanneldTypes.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "ILiveCodingModule.h"
#include "Async/Async.h"
#include "ChanneldEditorTypes.h"

IMPLEMENT_MODULE(FChanneldEditorModule, ChanneldEditor);

#define LOCTEXT_NAMESPACE "FChanneldUEModule"

void FChanneldEditorModule::StartupModule()
{
	FChanneldEditorStyle::Initialize();
	FChanneldEditorStyle::ReloadTextures();

	FChanneldEditorCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);
	PluginCommands->MapAction(
		FChanneldEditorCommands::Get().PluginCommand,
		FExecuteAction::CreateRaw(this, &FChanneldEditorModule::LaunchChanneldAndServersAction));
	PluginCommands->MapAction(
		FChanneldEditorCommands::Get().ToggleNetworkingCommand,
		FExecuteAction::CreateStatic(&FChanneldEditorModule::ToggleNetworkingAction),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&FChanneldEditorModule::IsNetworkingEnabled));
	PluginCommands->MapAction(
		FChanneldEditorCommands::Get().LaunchChanneldCommand,
		FExecuteAction::CreateRaw(this, &FChanneldEditorModule::LaunchChanneldAction));
	PluginCommands->MapAction(
		FChanneldEditorCommands::Get().StopChanneldCommand,
		FExecuteAction::CreateRaw(this, &FChanneldEditorModule::StopChanneldAction));
	PluginCommands->MapAction(
		FChanneldEditorCommands::Get().LaunchServersCommand,
		FExecuteAction::CreateRaw(this, &FChanneldEditorModule::LaunchServersAction));
	PluginCommands->MapAction(
		FChanneldEditorCommands::Get().ServerSettingsCommand,
		FExecuteAction::CreateRaw(this, &FChanneldEditorModule::OpenEditorSettingsAction));
	PluginCommands->MapAction(
		FChanneldEditorCommands::Get().StopServersCommand,
		FExecuteAction::CreateRaw(this, &FChanneldEditorModule::StopServersAction));
	PluginCommands->MapAction(
		FChanneldEditorCommands::Get().ToggleCompatibleRecompilationCommand,
		FExecuteAction::CreateStatic(&FChanneldEditorModule::ToggleCompatibleRecompilationAction),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&FChanneldEditorModule::IsCompatibleRecompilationEnabled));
	PluginCommands->MapAction(
		FChanneldEditorCommands::Get().GenerateReplicatorCommand,
		FExecuteAction::CreateRaw(this, &FChanneldEditorModule::GenerateReplicationAction));
	PluginCommands->MapAction(
		FChanneldEditorCommands::Get().AddRepComponentsToBPsCommand,
		FExecuteAction::CreateRaw(this, &FChanneldEditorModule::AddRepCompsToBPsAction));
	PluginCommands->MapAction(
		FChanneldEditorCommands::Get().OpenChannelDataEditorCommand,
		FExecuteAction::CreateRaw(this, &FChanneldEditorModule::OpenChannelDataEditorAction));
	PluginCommands->MapAction(
		FChanneldEditorCommands::Get().OpenCloudDeploymentCommand,
		FExecuteAction::CreateRaw(this, &FChanneldEditorModule::OpenCloudDeploymentAction));

#if ENGINE_MAJOR_VERSION == 5
	UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User");
	FToolMenuSection& Section = ToolbarMenu->AddSection("ChanneldComboButton");
	Section.AddEntry(FToolMenuEntry::InitComboButton(
		"ChanneldComboButton",
		FUIAction(),
		FOnGetContent::CreateRaw(this,&FChanneldEditorModule::CreateMenuContent, PluginCommands),
		LOCTEXT("LevelEditorToolbarChanneldButtonLabel", "Channeld"),
		LOCTEXT("LevelEditorToolbarChanneldButtonTooltip", "Tools and utilities provided by ChanneldUE"),
		FSlateIcon(FChanneldEditorStyle::GetStyleSetName(), "ChanneldEditor.PluginCommand")
	));
#else
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender());
	ToolbarExtender->AddToolBarExtension("Compile", EExtensionHook::After, PluginCommands,
										 FToolBarExtensionDelegate::CreateRaw(this, &FChanneldEditorModule::AddToolbarButton));

	LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
#endif
	// Stop all services launched during the session 
	FEditorDelegates::EditorModeIDExit.AddLambda([&](const FEditorModeID&)
	{
		StopChanneldAction();
		StopServersAction();
	});

	// Add editor settings 
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Editor", "Plugins", "ChanneldEditorSettings",
		                                 LOCTEXT("EditorSettingsName", "Channeld Editor"),
		                                 LOCTEXT("EditorSettingsDesc", ""),
		                                 GetMutableDefault<UChanneldEditorSettings>());
	}

	// Register the custom property type layout for the FClientInterestSettingsPreset struct in Project Settings.
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	// PropertyModule.RegisterCustomPropertyTypeLayout(FClientInterestSettingsPreset::StaticStruct()->GetFName(),
	// 	FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FClientInterestSettingsCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(UChanneldSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FChanneldSettingsDetails::MakeInstance));

	PropertyModule.NotifyCustomizationModuleChanged();

	BuildChanneldNotify = NewObject<UChanneldMissionNotiProxy>();
	BuildChanneldNotify->AddToRoot();

	AddRepCompNotify = NewObject<UChanneldMissionNotiProxy>();
	AddRepCompNotify->AddToRoot();
}

void FChanneldEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(FClientInterestSettingsPreset::StaticStruct()->GetFName());
		PropertyModule.UnregisterCustomClassLayout(UChanneldSettings::StaticClass()->GetFName());
		PropertyModule.NotifyCustomizationModuleChanged();
	}

	FChanneldEditorStyle::Shutdown();

	FChanneldEditorCommands::Unregister();
}

void FChanneldEditorModule::AddToolbarButton(FToolBarBuilder& Builder)
{
#if ENGINE_MAJOR_VERSION >= 5 
	UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User");
	FToolMenuSection& Section = ToolbarMenu->AddSection("ChanneldComboButton");
	Section.AddEntry(FToolMenuEntry::InitComboButton(
		"ChanneldComboButton",
		FUIAction(),
		FOnGetContent::CreateRaw(this,&FChanneldEditorModule::CreateMenuContent, PluginCommands)));
#else
	Builder.AddToolBarButton(FChanneldEditorCommands::Get().PluginCommand);

	FUIAction TempAction;
	Builder.AddComboButton(TempAction,
	                       FOnGetContent::CreateRaw(this, &FChanneldEditorModule::CreateMenuContent, PluginCommands),
	                       LOCTEXT("ChanneldComboButton", "Channeld combo button"),
	                       LOCTEXT("ChanneldComboButtonTootlip", "Channeld combo button tootltip"),
	                       TAttribute<FSlateIcon>(),
	                       true
	);
#endif
}

TSharedRef<SWidget> FChanneldEditorModule::CreateMenuContent(TSharedPtr<FUICommandList> Commands)
{
	FMenuBuilder MenuBuilder(true, Commands);

	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().ToggleNetworkingCommand);

	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().LaunchChanneldCommand);
	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().StopChanneldCommand);

	MenuBuilder.AddSeparator();

	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().LaunchServersCommand);
	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().ServerSettingsCommand);
	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().StopServersCommand);

	MenuBuilder.AddSeparator();

	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().OpenChannelDataEditorCommand);
	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().ToggleCompatibleRecompilationCommand);
	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().GenerateReplicatorCommand);

	MenuBuilder.AddSeparator();

	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().OpenCloudDeploymentCommand);

	MenuBuilder.AddSubMenu(LOCTEXT("ChanneldAdvancedHeading", "Advanced..."),
	                       LOCTEXT("ChanneldAdvancedTooltip", ""), FNewMenuDelegate::CreateLambda([](FMenuBuilder& InMenuBuilder)
	                       {
		                       InMenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().AddRepComponentsToBPsCommand);
	                       }));

	return MenuBuilder.MakeWidget();
}

bool FChanneldEditorModule::IsNetworkingEnabled()
{
	return GetMutableDefault<UChanneldSettings>()->IsNetworkingEnabled();
}

void FChanneldEditorModule::ToggleNetworkingAction()
{
	GetMutableDefault<UChanneldSettings>()->ToggleNetworkingEnabled();
}

void FChanneldEditorModule::LaunchChanneldAction()
{
	LaunchChanneldAction([](bool IsLaunched){});
}

void FChanneldEditorModule::LaunchChanneldAction(TFunction<void(bool IsLaunched)> PostChanneldLaunched /* nullptr*/)
{
	if(GEditor->GetEditorSubsystem<UChanneldEditorSubsystem>()->NeedToGenerateReplicationCode(true))
	{
		PostChanneldLaunched(false);
		return;
	}
	BuildChanneldNotify->SetMissionNotifyText(
		FText::FromString(TEXT("Building Channeld Gateway...")),
		LOCTEXT("RunningNotificationCancelButton", "Cancel"),
		FText::FromString(TEXT("Launch Channeld Gateway")),
		FText::FromString(TEXT("Failed To Build Channeld Gateway!"))
	);
	BuildChanneldNotify->SpawnRunningMissionNotification(nullptr);

	if (FPlatformProcess::IsProcRunning(ChanneldProcHandle))
	{
		UE_LOG(LogChanneldEditor, Warning, TEXT("Channeld is already running"));
		PostChanneldLaunched(true);
		BuildChanneldNotify->SpawnMissionSucceedNotification(nullptr);
		return;
	}
	if (BuildChanneldWorkThread.IsValid() && BuildChanneldWorkThread->IsProcRunning())
	{
		UE_LOG(LogChanneldEditor, Warning, TEXT("Channeld is already building"));
		PostChanneldLaunched(false);
		return;
	}
	FString ChanneldPath = FPlatformMisc::GetEnvironmentVariable(TEXT("CHANNELD_PATH"));
	if (ChanneldPath.IsEmpty())
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("CHANNELD_PATH environment variable is not set. Please set it to the path of the channeld source code directory."));
		BuildChanneldNotify->SpawnMissionFailedNotification(nullptr);
		return;
	}
	FPaths::NormalizeDirectoryName(ChanneldPath);
	const FString WorkingDir = ChanneldPath;
	const UChanneldEditorSettings* Settings = GetMutableDefault<UChanneldEditorSettings>();

	FString LaunchChanneldEntryDir = ChanneldPath / Settings->LaunchChanneldEntry;
	FPaths::NormalizeDirectoryName(LaunchChanneldEntryDir);
	if (!FPaths::DirectoryExists(LaunchChanneldEntryDir))
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Launch channeld entry is not a valid folder. %s"), *(LaunchChanneldEntryDir));
		BuildChanneldNotify->SpawnMissionFailedNotification(nullptr);
		return;
	}
	const FString GoBuildArgs = FString::Printf(TEXT("build -o . \"%s\""), *(LaunchChanneldEntryDir / TEXT("...")));
	FString LaunchChanneldParameterStr = TEXT("");
	for (const FString& LaunchChanneldParameter : Settings->LaunchChanneldParameters)
	{
		LaunchChanneldParameterStr.Append(LaunchChanneldParameter + TEXT(" "));
	}
	const FString RunChanneldArgs = FString::Printf(TEXT("%s"), *LaunchChanneldParameterStr);
	const FString ChanneldBinPath = ChanneldPath / FPaths::GetCleanFilename(LaunchChanneldEntryDir) + TEXT(".exe");

	BuildChanneldWorkThread = MakeShareable(
		new FChanneldProcWorkerThread(
			TEXT("ChanneldBuildWorkThread"),
			TEXT("go"),
			GoBuildArgs,
			WorkingDir
		)
	);
	BuildChanneldWorkThread->ProcFailedDelegate.AddUObject(BuildChanneldNotify, &UChanneldMissionNotiProxy::SpawnMissionFailedNotification);
	BuildChanneldWorkThread->ProcOutputMsgDelegate.BindUObject(BuildChanneldNotify, &UChanneldMissionNotiProxy::ReceiveOutputMsg);
	BuildChanneldWorkThread->ProcSucceedDelegate.AddLambda([this, ChanneldBinPath, RunChanneldArgs, WorkingDir, PostChanneldLaunched](FChanneldProcWorkerThread*)
	{
		BuildChanneldNotify->SpawnMissionSucceedNotification(nullptr);
		uint32 ProcessId;
		ChanneldProcHandle = FPlatformProcess::CreateProc(
			*ChanneldBinPath,
			*RunChanneldArgs,
			false, false, false, &ProcessId, 0, *WorkingDir, nullptr, nullptr
		);

		FPlatformProcess::Sleep(0.5f);
		if (!FPlatformProcess::IsProcRunning(ChanneldProcHandle))
		{
			// If the channeld process is not running, it means that it failed to launch.
			// We need to run again and read the error.
			void* ReadPipe = nullptr;
			void* WritePipe = nullptr;
			FPlatformProcess::CreatePipe(ReadPipe, WritePipe);
			ChanneldProcHandle = FPlatformProcess::CreateProc(
				*ChanneldBinPath,
				*RunChanneldArgs,
				false, true, true, &ProcessId, 0, *WorkingDir, WritePipe, ReadPipe
			);
			FPlatformProcess::Sleep(0.5f);
			const FString ErrOutput = FPlatformProcess::ReadPipe(ReadPipe);
			FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
			UE_LOG(LogChanneldEditor, Error, TEXT("Failed to launch channeld:\n%s"), *ErrOutput)
			// Open message dialog on game thread, because we are in a worker thread.
			AsyncTask(ENamedThreads::GameThread, [this, ErrOutput, PostChanneldLaunched]()
			{
				const FText Title = LOCTEXT("ChanneldLaunchFailedTitle", "Launch Channeld Failed");
				FMessageDialog::Open(
					EAppMsgType::Ok,
					FText::Format(
						LOCTEXT("ChanneldLaunchFailedText", "Failed to launch channeld:\n{0}"),
						FText::FromString(ErrOutput)
					),
					&Title
				);
				if (PostChanneldLaunched != nullptr)
					PostChanneldLaunched(false);
			});
		}
		else
		{
			if (PostChanneldLaunched != nullptr)
				PostChanneldLaunched(true);
		}
	});

	BuildChanneldNotify->MissionCanceled.AddLambda([this]()
	{
		if (BuildChanneldWorkThread.IsValid() && BuildChanneldWorkThread->GetThreadStatus() == EChanneldThreadStatus::Busy)
		{
			BuildChanneldWorkThread->Cancel();
		}
	});

	BuildChanneldWorkThread->Execute();
}

void FChanneldEditorModule::StopChanneldAction()
{
	if (FPlatformProcess::IsProcRunning(ChanneldProcHandle))
	{
		FPlatformProcess::TerminateProc(ChanneldProcHandle, true);
		ChanneldProcHandle.Reset();
		UE_LOG(LogChanneldEditor, Log, TEXT("Stopped channeld"));
	}
}

FTimerManager* FChanneldEditorModule::GetTimerManager()
{
	return &GEditor->GetTimerManager().Get();
}

void FChanneldEditorModule::LaunchServersAction()
{
	if(GEditor->GetEditorSubsystem<UChanneldEditorSubsystem>()->NeedToGenerateReplicationCode(true))
	{
		return;
	}
	UChanneldEditorSettings* Settings = GetMutableDefault<UChanneldEditorSettings>();
	FTimerManager* TimerManager = GetTimerManager();
	for (FServerGroup& ServerGroup : Settings->ServerGroups)
	{
		if (!ServerGroup.bEnabled)
			continue;

		if (TimerManager)
		{
			TimerManager->ClearTimer(ServerGroup.DelayHandle);
		}

		if (ServerGroup.DelayTime > 0)
		{
			if (TimerManager)
			{
				TimerManager->SetTimer(ServerGroup.DelayHandle, [&, ServerGroup]()
				{
					LaunchServerGroup(ServerGroup);
				}, ServerGroup.DelayTime, false, ServerGroup.DelayTime);
			}
			else
			{
				UE_LOG(LogChanneldEditor, Error, TEXT("Unable to find any TimerManager to delay the server launch."))
			}
		}
		else
		{
			LaunchServerGroup(ServerGroup);
		}
	}
}

void FChanneldEditorModule::LaunchServerGroup(const FServerGroup& ServerGroup)
{
	const FString EditorPath = FString(FPlatformProcess::ExecutablePath());
	const FString ProjectPath = FPaths::GetProjectFilePath();
	auto Settings = GetMutableDefault<UChanneldSettings>();

	// If server map is not set, use current level.
	FString MapName = ServerGroup.ServerMap.IsValid() ? ServerGroup.ServerMap.GetLongPackageName() : GEditor->GetEditorWorldContext().World()->GetOuter()->GetName();
	FString ViewClassName = ServerGroup.ServerViewClass ? ServerGroup.ServerViewClass->GetPathName() : Settings->ChannelDataViewClass->GetPathName();

	for (int i = 0; i < ServerGroup.ServerNum; i++)
	{
		FString Params = FString::Printf(TEXT("\"%s\" %s -game -PIEVIACONSOLE -Multiprocess -server -log -MultiprocessSaveConfig -forcepassthrough -channeld=%s -SessionName=\"%s - Server %d\" -windowed ViewClass=%s %s"),
		                                 *ProjectPath, *MapName, Settings->bEnableNetworking ? TEXT("True") : TEXT("False"), *MapName, i, *ViewClassName, *ServerGroup.AdditionalArgs.ToString());
		uint32 ProcessId;
		FProcHandle ProcHandle = FPlatformProcess::CreateProc(*EditorPath, *Params, true, false, false, &ProcessId, 0, nullptr, nullptr, nullptr);
		if (ProcHandle.IsValid())
		{
			ServerProcHandles.Add(ProcHandle);
		}
	}
}

bool FChanneldEditorModule::IsCompatibleRecompilationEnabled()
{
	return GetMutableDefault<UChanneldEditorSettings>()->bEnableCompatibleRecompilation;
}

void FChanneldEditorModule::ToggleCompatibleRecompilationAction()
{
	UChanneldEditorSettings* Settings = GetMutableDefault<UChanneldEditorSettings>();
	Settings->bEnableCompatibleRecompilation = !Settings->bEnableCompatibleRecompilation;
	Settings->SaveConfig();
}

void FChanneldEditorModule::StopServersAction()
{
	if (FTimerManager* TimerManager = GetTimerManager())
	{
		for (FServerGroup& ServerGroup : GetMutableDefault<UChanneldEditorSettings>()->ServerGroups)
		{
			TimerManager->ClearTimer(ServerGroup.DelayHandle);
		}
	}

	for (FProcHandle& ServerProc : ServerProcHandles)
	{
		FPlatformProcess::TerminateProc(ServerProc, true);
	}
	ServerProcHandles.Reset();
}

void FChanneldEditorModule::LaunchChanneldAndServersAction()
{
	LaunchChanneldAction([this](bool IsLaunched)
	{
		if (IsLaunched)
		{
			AsyncTask(ENamedThreads::GameThread, [this]()
			{
				LaunchServersAction();
			});
		}
	});
}

void FChanneldEditorModule::GenerateReplicationAction()
{
	GEditor->GetEditorSubsystem<UChanneldEditorSubsystem>()->GenerateReplicationAction();
}

void FChanneldEditorModule::OpenEditorSettingsAction()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->ShowViewer("Editor", "Plugins", "ChanneldEditorSettings");
	}
}

void FChanneldEditorModule::AddRepCompsToBPsAction()
{
	TSubclassOf<class UChanneldReplicationComponent> CompClass = GetMutableDefault<UChanneldEditorSettings>()->DefaultReplicationComponent;
	GEditor->GetEditorSubsystem<UAddCompToBPSubsystem>()->AddComponentToBlueprints(CompClass, FName(TEXT("ChanneldRepComp")));
}

void FChanneldEditorModule::OpenChannelDataEditorAction()
{
	GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>()->SpawnAndRegisterTab(LoadObject<UEditorUtilityWidgetBlueprint>(nullptr, L"/ChanneldUE/EditorUtilityWidgets/ChannelDataSchemaEditor"));
}

void FChanneldEditorModule::OpenCloudDeploymentAction()
{
	GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>()->SpawnAndRegisterTab(LoadObject<UEditorUtilityWidgetBlueprint>(nullptr, L"/ChanneldUE/EditorUtilityWidgets/CloudDeployment"));
}

#undef LOCTEXT_NAMESPACE
