#include "ChanneldEditor.h"

#include "BlueprintCompilationManager.h"
#include "ChanneldEditorCommands.h"
#include "ChanneldEditorSettings.h"
#include "ChanneldEditorStyle.h"
#include "ChanneldMissionNotiProxy.h"
#include "AddCompToBPSubsystem.h"
#include "ChanneldSettings.h"
#include "LevelEditor.h"
#include "ReplicatorGeneratorManager.h"
#include "ReplicatorGeneratorUtils.h"
#include "ChanneldUE/Replication/ChanneldReplicationComponent.h"
#include "Commandlets/CommandletHelpers.h"
#include "Widgets/Input/SSpinBox.h"
#include "ThreadUtils/FChanneldProcWorkerThread.h"


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
		FChanneldEditorCommands::Get().StopServersCommand,
		FExecuteAction::CreateRaw(this, &FChanneldEditorModule::StopServersAction));
	PluginCommands->MapAction(
		FChanneldEditorCommands::Get().GenerateReplicatorCommand,
		FExecuteAction::CreateRaw(this, &FChanneldEditorModule::GenerateReplicatorAction));
	PluginCommands->MapAction(
		FChanneldEditorCommands::Get().AddReplicationComponentCommand,
		FExecuteAction::CreateRaw(this, &FChanneldEditorModule::AddRepCompToBPAction));

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

	GenRepMissionNotifyProxy = NewObject<UChanneldMissionNotiProxy>();
	GenRepMissionNotifyProxy->AddToRoot();
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
	Builder.AddMenuEntry(FChanneldEditorCommands::Get().StopServersCommand);
	Builder.AddMenuEntry(FChanneldEditorCommands::Get().GenerateReplicatorCommand);
}

TSharedRef<SWidget> FChanneldEditorModule::CreateMenuContent(TSharedPtr<FUICommandList> Commands)
{
	FMenuBuilder MenuBuilder(true, Commands);

	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().LaunchChanneldCommand);
	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().StopChanneldCommand);

	MenuBuilder.AddSeparator();

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

	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().LaunchServersCommand);
	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().StopServersCommand);

	MenuBuilder.AddSeparator();

	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().AddReplicationComponentCommand);

	MenuBuilder.AddSeparator();

	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().GenerateReplicatorCommand);

	return MenuBuilder.MakeWidget();
}

void FChanneldEditorModule::LaunchChanneldAction()
{
	UE_LOG(LogTemp, Warning, TEXT("LaunchChanneldAction is not implemented yet"));
}

void FChanneldEditorModule::StopChanneldAction()
{
	UE_LOG(LogTemp, Warning, TEXT("LaunchChanneldAction is not implemented yet"));
}

void FChanneldEditorModule::LaunchServersAction()
{
	FString EditorPath = FString(FPlatformProcess::ExecutablePath());
	FString ProjectPath = FPaths::GetProjectFilePath();

	int ServerNum = UChanneldEditorSettings::GetServerNum();
	if (ServerNum <= 0) ServerNum = 1;

	FString MapName = UChanneldEditorSettings::GetServerMapName().ToString();
	if (MapName.Len() == 0) MapName = GEditor->GetEditorWorldContext().World()->GetMapName();

	for (int i = 0; i < ServerNum; i++)
	{
		FString Params = FString::Printf(TEXT("\"%s\" /Game/Maps/%s -game -PIEVIACONSOLE -Multiprocess -server -log -MultiprocessSaveConfig -forcepassthrough -SessionName=\"Dedicated Server %d\" -windowed %s"), *ProjectPath, *MapName, i, *UChanneldEditorSettings::GetAdditionalArgs().ToString());
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
	for (FProcHandle& ServerProc : ServerProcHandles)
	{
		FPlatformProcess::TerminateProc(ServerProc, true);
	}
	ServerProcHandles.Reset();
}

void FChanneldEditorModule::GenerateReplicatorAction()
{
	FString MissionName = TEXT("CookAndGenRep");
	FString Command = CommandletHelpers::BuildCommandletProcessArguments(*MissionName, *FPaths::GetProjectFilePath(), TEXT(" -targetplatform=WindowsServer -skipcompile -SkipShaderCompile -nop4 -cook -skipstage -utf8output -stdout"));
	FString Cmd = ChanneldReplicatorGeneratorUtils::GetUECmdBinary();
	GenRepWorkThread = MakeShareable(new FChanneldProcWorkerThread(TEXT("CookAndGenRepThread"), Cmd, Command));
	GenRepWorkThread->ProcOutputMsgDelegate.BindUObject(GenRepMissionNotifyProxy, &UChanneldMissionNotiProxy::ReceiveOutputMsg);
	GenRepWorkThread->ProcBeginDelegate.AddUObject(GenRepMissionNotifyProxy, &UChanneldMissionNotiProxy::SpawnRuningMissionNotification);
	GenRepWorkThread->ProcSuccessedDelegate.AddUObject(GenRepMissionNotifyProxy, &UChanneldMissionNotiProxy::SpawnMissionSuccessedNotification);
	GenRepWorkThread->ProcFaildDelegate.AddUObject(GenRepMissionNotifyProxy, &UChanneldMissionNotiProxy::SpawnMissionFaildNotification);
	GenRepMissionNotifyProxy->SetMissionName(*FString::Printf(TEXT("%s"), *MissionName));
	GenRepMissionNotifyProxy->SetMissionNotifyText(
		FText::FromString(FString::Printf(TEXT("%s in progress"), *MissionName)),
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

void FChanneldEditorModule::AddRepCompToBPAction()
{
	TSubclassOf<class UChanneldReplicationComponent> CompClass = GetMutableDefault<UChanneldSettings>()->DefaultReplicationComponent;
	GEditor->GetEditorSubsystem<UAddCompToBPSubsystem>()->AddComponentToActorBlueprint(CompClass, FName(TEXT("ChanneldRepComp")));
}

IMPLEMENT_MODULE(FChanneldEditorModule, ChanneldEditor)

#undef LOCTEXT_NAMESPACE
