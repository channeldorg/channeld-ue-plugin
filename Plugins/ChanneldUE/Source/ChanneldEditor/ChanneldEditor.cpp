#include "ChanneldEditor.h"
#include "ChanneldEditorCommands.h"
#include "LevelEditor.h"
#include "Widgets/Input/SSpinBox.h"
#include "ChanneldEditorSettings.h"
#include "ChanneldEditorStyle.h"

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
		FChanneldEditorCommands::Get().LaunchChanneldCommand,
		FExecuteAction::CreateRaw(this, &FChanneldEditorModule::StopChanneldAction));
	PluginCommands->MapAction(
		FChanneldEditorCommands::Get().LaunchServersCommand,
		FExecuteAction::CreateRaw(this, &FChanneldEditorModule::LaunchServersAction));
	PluginCommands->MapAction(
		FChanneldEditorCommands::Get().StopServersCommand,
		FExecuteAction::CreateRaw(this, &FChanneldEditorModule::StopServersAction));

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
	
	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().LaunchServersCommand);
	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().StopServersCommand);

	return MenuBuilder.MakeWidget();
}

void FChanneldEditorModule::LaunchChanneldAction()
{
	UE_LOG(LogTemp, Warning, TEXT("Not implemented yet"));
}

void FChanneldEditorModule::StopChanneldAction()
{
	UE_LOG(LogTemp, Warning, TEXT("Not implemented yet"));
}

void FChanneldEditorModule::LaunchServersAction()
{
	FString EditorPath = FString(FPlatformProcess::ExecutablePath());
	FString ProjectPath = FPaths::GetProjectFilePath();

	int ServerNum = UChanneldEditorSettings::GetServerNum();
	if (ServerNum <= 0)	ServerNum = 1;

	FString MapName = UChanneldEditorSettings::GetServerMapName().ToString();
	if (MapName.Len() == 0) MapName = GEditor->GetEditorWorldContext().World()->GetMapName();

	for (int i = 0; i < ServerNum; i++)
	{
		FString Params = FString::Printf(TEXT("\"%s\" /Game/Maps/%s -game -PIEVIACONSOLE -Multiprocess -server -log -MultiprocessSaveConfig -forcepassthrough -messaging -SessionName=\"Dedicated Server %d\" -windowed"), *ProjectPath, *MapName, i);
		uint32 ProcessId;
		FProcHandle ProcHandle = FPlatformProcess::CreateProc(*EditorPath, *Params, true, true, false, &ProcessId, 0, nullptr, nullptr, nullptr);
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

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FChanneldEditorModule, ChanneldEditor)