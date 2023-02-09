#include "ChanneldEditor.h"

#include "BlueprintCompilationManager.h"
#include "ChanneldEditorCommands.h"
#include "ChanneldEditorSettings.h"
#include "ChanneldEditorStyle.h"
#include "ChanneldMissionNotiProxy.h"
#include "AddCompToBPSubsystem.h"
#include "ChanneldProtobufEditor.h"
#include "ChanneldSettings.h"
#include "LevelEditor.h"
#include "ReplicatorGeneratorManager.h"
#include "ReplicatorGeneratorUtils.h"
#include "ChanneldUE/Replication/ChanneldReplicationComponent.h"
#include "Commandlets/CommandletHelpers.h"
#include "Widgets/Input/SSpinBox.h"
#include "ThreadUtils/FChanneldProcWorkerThread.h"
#include "ISettingsModule.h"

#define LOCTEXT_NAMESPACE "FChanneldUEModule"

void FChanneldEditorModule::StartupModule()
{
	FChanneldEditorStyle::Initialize();
	FChanneldEditorStyle::ReloadTextures();

	FChanneldEditorCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);
	PluginCommands->MapAction(
		FChanneldEditorCommands::Get().PluginCommand,
		FExecuteAction::CreateRaw(this, &FChanneldEditorModule::LaunchServersAction));
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
		FChanneldEditorCommands::Get().GenerateReplicatorCommand,
		FExecuteAction::CreateRaw(this, &FChanneldEditorModule::GenerateReplicatorAction));
	PluginCommands->MapAction(
		FChanneldEditorCommands::Get().AddRepComponentsToBPsCommand,
		FExecuteAction::CreateRaw(this, &FChanneldEditorModule::AddRepCompsToBPsAction));

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender());
	ToolbarExtender->AddToolBarExtension("Compile", EExtensionHook::After, PluginCommands,
	                                     FToolBarExtensionDelegate::CreateRaw(this, &FChanneldEditorModule::AddToolbarButton));
	LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);

	//TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
	//MenuExtender->AddMenuExtension("LevelEditor", EExtensionHook::After, PluginCommands,
	//	FMenuExtensionDelegate::CreateRaw(this, &FChanneldEditorModule::AddMenuEntry));
	//LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(MenuExtender);

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

	GenRepMissionNotifyProxy = NewObject<UChanneldMissionNotiProxy>();
	GenRepMissionNotifyProxy->AddToRoot();

	AddRepCompMissionNotifyProxy = NewObject<UChanneldMissionNotiProxy>();
	AddRepCompMissionNotifyProxy->AddToRoot();
}

void FChanneldEditorModule::ShutdownModule()
{
	FChanneldEditorStyle::Shutdown();

	FChanneldEditorCommands::Unregister();
}

void FChanneldEditorModule::AddToolbarButton(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(FChanneldEditorCommands::Get().PluginCommand);

	FUIAction TempAction;
	Builder.AddComboButton(TempAction,
	                       FOnGetContent::CreateRaw(this, &FChanneldEditorModule::CreateMenuContent, PluginCommands),
	                       LOCTEXT("ChanneldComboButton", "Channeld combo button"),
	                       LOCTEXT("ChanneldComboButtonTootlip", "Channeld combo button tootltip"),
	                       TAttribute<FSlateIcon>(),
	                       true
	);
}

void FChanneldEditorModule::AddMenuEntry(FMenuBuilder& Builder)
{
	Builder.BeginSection("Channeld", TAttribute<FText>(FText::FromString("Channeld")));

	auto Cmd = FChanneldEditorCommands::Get().LaunchChanneldCommand;
	Builder.AddMenuEntry(FChanneldEditorCommands::Get().LaunchChanneldCommand);
	Builder.AddSubMenu(FText::FromString("Channeld submenu"),
	                   FText::FromString("Channeld submenu tooltip"),
	                   FNewMenuDelegate::CreateRaw(this, &FChanneldEditorModule::FillSubmenu));

	Builder.EndSection();
}

void FChanneldEditorModule::FillSubmenu(FMenuBuilder& Builder)
{
	Builder.AddMenuEntry(FChanneldEditorCommands::Get().LaunchChanneldCommand);
	Builder.AddMenuEntry(FChanneldEditorCommands::Get().StopChanneldCommand);
	Builder.AddMenuEntry(FChanneldEditorCommands::Get().LaunchServersCommand);
	Builder.AddMenuEntry(FChanneldEditorCommands::Get().ServerSettingsCommand);
	Builder.AddMenuEntry(FChanneldEditorCommands::Get().StopServersCommand);
	Builder.AddMenuEntry(FChanneldEditorCommands::Get().GenerateReplicatorCommand);
}

TSharedRef<SWidget> FChanneldEditorModule::CreateMenuContent(TSharedPtr<FUICommandList> Commands)
{
	FMenuBuilder MenuBuilder(true, Commands);

	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().LaunchChanneldCommand);
	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().StopChanneldCommand);

	MenuBuilder.AddSeparator();

	/*
	TSharedRef<SWidget> NumServers = SNew(SSpinBox<int32>)
		.MinValue(1)
		.MaxValue(16)
		.MinSliderValue(1)
		.MaxSliderValue(4)
		.Delta(1)
		.MinDesiredWidth(200)
		.ContentPadding(FMargin(2, 0))
		.ToolTipText(LOCTEXT("NumberOfServersToolTip", "How many server instances do you want to create?"))
		.Value_Static(&UChanneldEditorSettings::GetServerNum)
		.OnValueChanged_Static(&UChanneldEditorSettings::SetServerNum);
	MenuBuilder.AddWidget(NumServers, LOCTEXT("NumServersLabel", "Number of Servers"));

	//FEditableTextStyle* EditableTextStyle = new FEditableTextStyle;
	//EditableTextStyle->ColorAndOpacity = FSlateColor(FLinearColor(1.0F, 1.0F, 1.0F));
	TSharedRef<SWidget> ServerMapName = SNew(SEditableText)
		//.Style(EditableTextStyle)
		.Text_Static(&UChanneldEditorSettings::GetServerMapName)
		.OnTextChanged_Static(&UChanneldEditorSettings::SetServerMapName);
	MenuBuilder.AddWidget(ServerMapName, LOCTEXT("ServerMapNameLabel", "Server Map Name:"));

	TSharedRef<SWidget> AdditonalArgs = SNew(SEditableText)
		//.Style(EditableTextStyle)
		.Text_Static(&UChanneldEditorSettings::GetAdditionalArgs)
		.OnTextChanged_Static(&UChanneldEditorSettings::SetAdditionalArgs);
	MenuBuilder.AddWidget(AdditonalArgs, LOCTEXT("AdditonalArgsLabel", "Additional Args:"));
	*/

	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().LaunchServersCommand);
	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().ServerSettingsCommand);
	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().StopServersCommand);

	MenuBuilder.AddSeparator();

	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().AddRepComponentsToBPsCommand);

	MenuBuilder.AddSeparator();

	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().GenerateReplicatorCommand);

	return MenuBuilder.MakeWidget();
}

void FChanneldEditorModule::LaunchChanneldAction()
{
	UE_LOG(LogChanneldEditor, Warning, TEXT("LaunchChanneldAction is not implemented yet"));
}

void FChanneldEditorModule::StopChanneldAction()
{
	UE_LOG(LogChanneldEditor, Warning, TEXT("LaunchChanneldAction is not implemented yet"));
}

FTimerManager* FChanneldEditorModule::GetTimerManager()
{
	return &GEditor->GetTimerManager().Get();
}

void FChanneldEditorModule::LaunchServersAction()
{
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

	// If server map is not set, use current level.
	FString MapName = ServerGroup.ServerMap.IsValid() ? ServerGroup.ServerMap.GetLongPackageName() : GEditor->GetEditorWorldContext().World()->GetOuter()->GetName();
	FString ViewClassName = ServerGroup.ServerViewClass ? ServerGroup.ServerViewClass->GetPathName() : GetMutableDefault<UChanneldSettings>()->ChannelDataViewClass->GetPathName();

	for (int i = 0; i < ServerGroup.ServerNum; i++)
	{
		FString Params = FString::Printf(TEXT("\"%s\" %s -game -PIEVIACONSOLE -Multiprocess -server -log -MultiprocessSaveConfig -forcepassthrough -SessionName=\"%s - Server %d\" -windowed ViewClass=%s %s"),
			*ProjectPath, *MapName, *MapName, i, *ViewClassName, *ServerGroup.AdditionalArgs.ToString());
		uint32 ProcessId;
		FProcHandle ProcHandle = FPlatformProcess::CreateProc(*EditorPath, *Params, true, false, false, &ProcessId, 0, nullptr, nullptr, nullptr);
		if (ProcHandle.IsValid())
		{
			ServerProcHandles.Add(ProcHandle);
		}
	}
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

void FChanneldEditorModule::GenerateReplicatorAction()
{
	FString MissionName = TEXT("CookAndGenerateReplicators");
	GenRepWorkThread = MakeShareable(
		new FChanneldProcWorkerThread(
			TEXT("CookAndGenRepThread"),
			ChanneldReplicatorGeneratorUtils::GetUECmdBinary(),
			CommandletHelpers::BuildCommandletProcessArguments(
				TEXT("CookAndGenRep"),
				*FString::Printf( TEXT("\"%s\""), *FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath())),
				TEXT(" -targetplatform=WindowsServer -skipcompile -nop4 -cook -skipstage -utf8output -stdout")
			)
		)
	);
	GenRepWorkThread->ProcOutputMsgDelegate.BindUObject(GenRepMissionNotifyProxy, &UChanneldMissionNotiProxy::ReceiveOutputMsg);
	GenRepWorkThread->ProcBeginDelegate.AddUObject(GenRepMissionNotifyProxy, &UChanneldMissionNotiProxy::SpawnRunningMissionNotification);
	GenRepWorkThread->ProcSucceedDelegate.AddRaw(this, &FChanneldEditorModule::GenReplicatorProto);
	GenRepWorkThread->ProcFailedDelegate.AddUObject(GenRepMissionNotifyProxy, &UChanneldMissionNotiProxy::SpawnMissionFailedNotification);
	GenRepMissionNotifyProxy->SetMissionName(*FString::Printf(TEXT("%s"), *MissionName));
	GenRepMissionNotifyProxy->SetMissionNotifyText(
		FText::FromString(TEXT("Cooking and generating replicators")),
		LOCTEXT("RunningCookNotificationCancelButton", "Cancel"),
		FText::FromString(FString::Printf(TEXT("%s Mission Finished!"), *MissionName)),
		FText::FromString(FString::Printf(TEXT("%s Failed!"), *MissionName))
	);
	GenRepMissionNotifyProxy->MissionCanceled.AddLambda([this]()
	{
		if (GenRepWorkThread.IsValid() && GenRepWorkThread->GetThreadStatus() == EChanneldThreadStatus::Busy)
		{
			GenRepWorkThread->Cancel();
		}
	});

	GenRepWorkThread->Execute();
}

void FChanneldEditorModule::GenReplicatorProto(FChanneldProcWorkerThread* ProcWorkerThread)
{
	TArray<FString> GeneratedProtoFiles = FReplicatorGeneratorManager::Get().GetGeneratedProtoFiles();
	FString ReplicatorStorageDir = FReplicatorGeneratorManager::Get().GetReplicatorStorageDir();
	FString ChanneldPath = FPlatformMisc::GetEnvironmentVariable(TEXT("CHANNELD_PATH"));
	if (ChanneldPath.IsEmpty())
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Environment variable \"CHANNELD_PATH\" is empty, please set environment variable \"CHANNELD_PATH\" to you system"));
		GenRepMissionNotifyProxy->SpawnMissionFailedNotification(ProcWorkerThread);
		return;
	}
	FString ChanneldUnrealpbPath = ChanneldPath / TEXT("pkg") / TEXT("unrealpb");
	FPaths::NormalizeDirectoryName(ChanneldUnrealpbPath);

	FString GameModuleExportAPIMacro = GetMutableDefault<UChanneldEditorSettings>()->GameModuleExportAPIMacro;

	if(GameModuleExportAPIMacro.IsEmpty())
	{
		UE_LOG(LogChanneldEditor, Verbose, TEXT("Game module export API macro is empty"));
	}

	FString Args = ChanneldProtobufHelpers::BuildProtocProcessArguments(
		ReplicatorStorageDir,
		 FString::Printf(TEXT("dllexport_decl=%s"), *GameModuleExportAPIMacro),
		{
			ReplicatorStorageDir,
			ChanneldUnrealpbPath,
		},
		GeneratedProtoFiles
	);
	
	IFileManager& FileManager = IFileManager::Get();
	FString ProtocPath = ChanneldProtobufHelpers::GetProtocPath();
	if (!FileManager.FileExists(*ProtocPath))
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Protoc path is invaild: %s"), *ProtocPath);
		GenRepMissionNotifyProxy->SpawnMissionFailedNotification(ProcWorkerThread);
		return;
	}

	GenProtoWorkThread = MakeShareable(new FChanneldProcWorkerThread(TEXT("GenerateReplicatorProtoThread"), ProtocPath, Args));
	GenProtoWorkThread->ProcOutputMsgDelegate.BindUObject(GenRepMissionNotifyProxy, &UChanneldMissionNotiProxy::ReceiveOutputMsg);
	GenProtoWorkThread->ProcBeginDelegate.AddUObject(GenRepMissionNotifyProxy, &UChanneldMissionNotiProxy::SpawnRunningMissionNotification);
	GenProtoWorkThread->ProcSucceedDelegate.AddLambda([this, GeneratedProtoFiles, ReplicatorStorageDir](FChanneldProcWorkerThread*)
	{
		IFileManager& FileManager = IFileManager::Get();
		for (FString GeneratedProtoFile : GeneratedProtoFiles)
		{
			GeneratedProtoFile = ReplicatorStorageDir / GeneratedProtoFile;
			FileManager.Move(
				*FPaths::ChangeExtension(GeneratedProtoFile, TEXT("pb.cpp")),
				*FPaths::ChangeExtension(GeneratedProtoFile, TEXT("pb.cc"))
			);
		}
		GenRepMissionNotifyProxy->SpawnMissionSucceedNotification(nullptr);
	});
	GenProtoWorkThread->ProcFailedDelegate.AddUObject(GenRepMissionNotifyProxy, &UChanneldMissionNotiProxy::SpawnMissionFailedNotification);
	GenRepMissionNotifyProxy->SetRunningNotifyText(FText::FromString(TEXT("Generating replicator protos")));
	GenRepMissionNotifyProxy->MissionCanceled.AddLambda([this]()
	{
		if (GenProtoWorkThread.IsValid() && GenProtoWorkThread->GetThreadStatus() == EChanneldThreadStatus::Busy)
		{
			GenProtoWorkThread->Cancel();
		}
	});

	GenProtoWorkThread->Execute();
}

void FChanneldEditorModule::AddRepCompsToBPsAction()
{
	TSubclassOf<class UChanneldReplicationComponent> CompClass = GetMutableDefault<UChanneldEditorSettings>()->DefaultReplicationComponent;
	GEditor->GetEditorSubsystem<UAddCompToBPSubsystem>()->AddComponentToBlueprints(CompClass, FName(TEXT("ChanneldRepComp")));
}

IMPLEMENT_MODULE(FChanneldEditorModule, ChanneldEditor)

void FChanneldEditorModule::OpenEditorSettingsAction()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->ShowViewer("Editor", "Plugins", "ChanneldEditorSettings");
	}
}

#undef LOCTEXT_NAMESPACE
