#include "ChanneldEditor.h"

#include "BlueprintCompilationManager.h"
#include "ChanneldEditorCommands.h"
#include "ChanneldEditorSettings.h"
#include "ChanneldEditorStyle.h"
#include "ChanneldMissionNotiProxy.h"
#include "AddCompToBPSubsystem.h"
#include "ChanneldSettings.h"
#include "ChanneldSettingsDetails.h"
#include "LevelEditor.h"
#include "ReplicatorGeneratorManager.h"
#include "ReplicatorGeneratorUtils.h"
#include "ChanneldUE/Replication/ChanneldReplicationComponent.h"
#include "Commandlets/CommandletHelpers.h"
#include "Widgets/Input/SSpinBox.h"
#include "ThreadUtils/FChanneldProcWorkerThread.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyEditorDelegates.h"
#include "ChanneldTypes.h"
#include "ILiveCodingModule.h"
#include "ProtocHelper.h"
#include "ReplicationRegistry.h"
#include "Async/Async.h"
#include "Misc/HotReloadInterface.h"

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

	GenRepNotify = NewObject<UChanneldMissionNotiProxy>();
	GenRepNotify->AddToRoot();

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

	MenuBuilder.AddMenuEntry(FChanneldEditorCommands::Get().GenerateReplicatorCommand);

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
	LaunchChanneldAction(nullptr);
}

void FChanneldEditorModule::LaunchChanneldAction(TFunction<void(EChanneldLaunchResult Result)> PostChanneldLaunched /* nullptr*/)
{
	if (FPlatformProcess::IsProcRunning(ChanneldProcHandle))
	{
		UE_LOG(LogChanneldEditor, Warning, TEXT("Channeld is already running"));
		PostChanneldLaunched(EChanneldLaunchResult::AlreadyLaunched);
		return;
	}
	if (BuildChanneldWorkThread.IsValid() && BuildChanneldWorkThread->IsProcRunning())
	{
		UE_LOG(LogChanneldEditor, Warning, TEXT("Channeld is already building"));
		PostChanneldLaunched(EChanneldLaunchResult::Building);
		return;
	}
	FString ChanneldPath = FPlatformMisc::GetEnvironmentVariable(TEXT("CHANNELD_PATH"));
	if (ChanneldPath.IsEmpty())
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("CHANNELD_PATH environment variable is not set. Please set it to the path of the channeld source code directory."));
		return;
	}
	FPaths::NormalizeDirectoryName(ChanneldPath);
	const FString WorkingDir = ChanneldPath;
	const UChanneldEditorSettings* Settings = GetMutableDefault<UChanneldEditorSettings>();
	
	const FString ChanneldBinPath = ChanneldPath / (FPaths::GetCleanFilename(Settings->LaunchChanneldEntry) + TEXT(".exe"));
	const FString GoBuildArgs = FString::Printf(TEXT("build -o . \"%s/...\""), *(ChanneldPath / *Settings->LaunchChanneldEntry));
	const FString RunChanneldArgs = FString::Printf(TEXT("%s"), *Settings->LaunchChanneldParameters);

	BuildChanneldNotify->SetMissionNotifyText(
		FText::FromString(TEXT("Building Channeld Gateway...")),
		LOCTEXT("RunningCookNotificationCancelButton", "Cancel"),
		FText::FromString(TEXT("Launch Channeld Gateway")),
		FText::FromString(TEXT("Failed To Build Channeld Gateway!"))
	);

	BuildChanneldWorkThread = MakeShareable(
		new FChanneldProcWorkerThread(
			TEXT("ChanneldBuildWorkThread"),
			TEXT("go"),
			GoBuildArgs,
			WorkingDir
		)
	);
	BuildChanneldWorkThread->ProcBeginDelegate.AddUObject(BuildChanneldNotify, &UChanneldMissionNotiProxy::SpawnRunningMissionNotification);
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
					PostChanneldLaunched(EChanneldLaunchResult::Failed);
			});
		}
		else
		{
			if (PostChanneldLaunched != nullptr)
				PostChanneldLaunched(EChanneldLaunchResult::Launched);
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
	LaunchChanneldAction([this](EChanneldLaunchResult Result)
	{
		if (Result < EChanneldLaunchResult::Failed)
		{
			AsyncTask(ENamedThreads::GameThread, [this]()
			{
				LaunchServersAction();
			});
		}
	});
}

void FChanneldEditorModule::GenerateReplicatorAction()
{
	// If developer has closed the ReplicationRegistryTable, we need to collect garbage to make sure the ReplicationRegistryTable asset is released.
	CollectGarbage(RF_NoFlags);
	// Make sure the ReplicationRegistryTable is saved and closed.
	// The CookAndGenRepCommandLet process will read and write ReplicationRegistryTable,
	// If the editor process is still holding the ReplicationRegistryTable, the CookAndGenRepCommandLet process will fail to write the ReplicationRegistryTable.
	if(!ReplicationRegistryUtils::PromptForSaveAndCloseRegistryTable())
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Please save and close the Replication Registry data table first"));
		return;
	}
	GenRepWorkThread = MakeShareable(
		new FChanneldProcWorkerThread(
			TEXT("CookAndGenRepThread"),
			ChanneldReplicatorGeneratorUtils::GetUECmdBinary(),
			CommandletHelpers::BuildCommandletProcessArguments(
				TEXT("CookAndGenRep"),
				*FString::Printf(TEXT("\"%s\""), *FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath())),
				*FString::Printf(TEXT(" -targetplatform=WindowsServer -skipcompile -nop4 -cook -skipstage -utf8output -stdout -GoPackageImportPathPrefix=%s"), *GetMutableDefault<UChanneldEditorSettings>()->ChanneldGoPackageImportPathPrefix)
			)
		)
	);
	GenRepWorkThread->ProcOutputMsgDelegate.BindUObject(GenRepNotify, &UChanneldMissionNotiProxy::ReceiveOutputMsg);
	GenRepWorkThread->ProcBeginDelegate.AddUObject(GenRepNotify, &UChanneldMissionNotiProxy::SpawnRunningMissionNotification);
	GenRepWorkThread->ProcSucceedDelegate.AddLambda([this](FChanneldProcWorkerThread* ProcWorkerThread)
	{
		FReplicatorGeneratorManager& GeneratorManager = FReplicatorGeneratorManager::Get();
		FGeneratedManifest LatestGeneratedManifest;
		FString Message;
		if (!GeneratorManager.LoadLatestGeneratedManifest(LatestGeneratedManifest, Message))
		{
			UE_LOG(LogChanneldEditor, Error, TEXT("Failed to load latest generated manifest: %s"), *Message);
			return;
		}

		const TArray<FString> GeneratedProtoFiles = FReplicatorGeneratorManager::Get().GetGeneratedProtoFiles();
		GenRepProtoCppCode(GeneratedProtoFiles);
		GenRepProtoGoCode(GeneratedProtoFiles, LatestGeneratedManifest, GeneratorManager.GetReplicatorStorageDir());


		// Update UChanneldSettings::DefaultChannelDataMsgNames
		auto Settings = GetMutableDefault<UChanneldSettings>();
		TMap<EChanneldChannelType, FString> NewChannelDataMsgNames;
		for (auto& Pair : Settings->DefaultChannelDataMsgNames)
		{
			NewChannelDataMsgNames.Add(Pair.Key, LatestGeneratedManifest.ChannelDataMsgName);
		}
		Settings->DefaultChannelDataMsgNames = NewChannelDataMsgNames;
		Settings->SaveConfig();
		UE_LOG(LogChanneldEditor, Log, TEXT("Updated the default channel data message names in the channeld settings."));

		GetMutableDefault<UChanneldSettings>()->ReloadConfig();
	});
	GenRepWorkThread->ProcFailedDelegate.AddUObject(GenRepNotify, &UChanneldMissionNotiProxy::SpawnMissionFailedNotification);
	GenRepNotify->SetMissionNotifyText(
		FText::FromString(TEXT("Cooking And Generating Replication Code...")),
		LOCTEXT("RunningCookNotificationCancelButton", "Cancel"),
		FText::FromString(TEXT("Successfully Generated Replication Code.")),
		FText::FromString(TEXT("Failed To Generate Replication Code!"))
	);
	GenRepNotify->MissionCanceled.AddLambda([this]()
	{
		if (GenRepWorkThread.IsValid() && GenRepWorkThread->GetThreadStatus() == EChanneldThreadStatus::Busy)
		{
			GenRepWorkThread->Cancel();
		}
	});

	GenRepWorkThread->Execute();
}

void FChanneldEditorModule::GenRepProtoCppCode(const TArray<FString>& ProtoFiles) const
{
	const FString ChanneldPath = FPlatformMisc::GetEnvironmentVariable(TEXT("CHANNELD_PATH"));
	if (ChanneldPath.IsEmpty())
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Environment variable \"CHANNELD_PATH\" is empty, please set user environment variable \"CHANNELD_PATH\" to Channeld root directory"));
		GenRepNotify->SpawnMissionFailedNotification(nullptr);
		return;
	}

	const FString GameModuleExportAPIMacro = GetMutableDefault<UChanneldEditorSettings>()->GameModuleExportAPIMacro;
	if (GameModuleExportAPIMacro.IsEmpty())
	{
		UE_LOG(LogChanneldEditor, Verbose, TEXT("Game module export API macro is empty"));
	}

	FString ReplicatorStorageDir = FReplicatorGeneratorManager::Get().GetReplicatorStorageDir();

	FString ChanneldUnrealpbPath = ChanneldPath / TEXT("pkg") / TEXT("unrealpb");
	FPaths::NormalizeDirectoryName(ChanneldUnrealpbPath);

	const FString Args = FProtocHelper::BuildProtocProcessCppArguments(
		ReplicatorStorageDir,
		FString::Printf(TEXT("dllexport_decl=%s"), *GameModuleExportAPIMacro),
		{
			ReplicatorStorageDir,
			ChanneldUnrealpbPath,
		},
		ProtoFiles
	);

	IFileManager& FileManager = IFileManager::Get();
	const FString ProtocPath = FProtocHelper::GetProtocPath();
	if (!FileManager.FileExists(*ProtocPath))
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Protoc path is invaild: %s"), *ProtocPath);
		GenRepNotify->SpawnMissionFailedNotification(nullptr);
		return;
	}

	UE_LOG(LogChanneldEditor, Display, TEXT("\"%s\" %s"), *ProtocPath, *Args);

	GenProtoCppCodeWorkThread = MakeShareable(new FChanneldProcWorkerThread(TEXT("GenerateReplicatorProtoThread"), ProtocPath, Args));
	GenProtoCppCodeWorkThread->ProcOutputMsgDelegate.BindUObject(GenRepNotify, &UChanneldMissionNotiProxy::ReceiveOutputMsg);
	GenProtoCppCodeWorkThread->ProcBeginDelegate.AddLambda([](FChanneldProcWorkerThread*)
		{
			UE_LOG(LogChanneldEditor, Display, TEXT("Start generating cpp prototype code..."));
		}
	);
	GenProtoCppCodeWorkThread->ProcFailedDelegate.AddLambda([this](FChanneldProcWorkerThread*)
		{
			UE_LOG(LogChanneldEditor, Error, TEXT("Failed to generate cpp proto codes!"));
			GenRepNotify->SpawnMissionFailedNotification(nullptr);
		}
	);
	GenRepNotify->MissionCanceled.AddLambda([this]()
	{
		if (GenProtoCppCodeWorkThread.IsValid() && GenProtoCppCodeWorkThread->GetThreadStatus() == EChanneldThreadStatus::Busy)
		{
			GenProtoCppCodeWorkThread->Cancel();
		}
	});
	GenProtoCppCodeWorkThread->ProcSucceedDelegate.AddLambda([this, ProtoFiles, ReplicatorStorageDir](FChanneldProcWorkerThread*)
		{
			IFileManager& FileManager = IFileManager::Get();
			for (FString GeneratedProtoFile : ProtoFiles)
			{
				GeneratedProtoFile = ReplicatorStorageDir / GeneratedProtoFile;
				FileManager.Move(
					*FPaths::ChangeExtension(GeneratedProtoFile, TEXT("pb.cpp")),
					*FPaths::ChangeExtension(GeneratedProtoFile, TEXT("pb.cc"))
				);
			}
			UE_LOG(LogChanneldEditor, Display, TEXT("Successfully generated cpp prototype code."));
			GenRepNotify->SpawnMissionSucceedNotification(nullptr);

			if (GetMutableDefault<UChanneldEditorSettings>()->bAutoRecompileAfterGenerate)
			{
				UE_LOG(LogChanneldEditor, Verbose, TEXT("Auto recompile game code after generate replicator protos"));
				// Run RecompileGameCode in game thread, the RecompileGameCode will use FNotificationInfo which can only be used in game thread
				AsyncTask(ENamedThreads::GameThread, [this]()
				{
					RecompileGameCode();
				});
			}
		}
	);

	GenProtoCppCodeWorkThread->Execute();
}

void FChanneldEditorModule::GenRepProtoGoCode(const TArray<FString>& ProtoFiles, const FGeneratedManifest& LatestGeneratedManifest, const FString& ReplicatorStorageDir) const
{
	const FString ChanneldPath = FPlatformMisc::GetEnvironmentVariable(TEXT("CHANNELD_PATH"));
	if (ChanneldPath.IsEmpty())
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Environment variable \"CHANNELD_PATH\" is empty, please set user environment variable \"CHANNELD_PATH\" to Channeld root directory"));
		return;
	}

	FString DirToGoMain = ChanneldPath / GetMutableDefault<UChanneldEditorSettings>()->ChanneldProtoFilesStorageDir;
	FString DirToGenGoProto = DirToGoMain / LatestGeneratedManifest.ProtoPackageName;
	FPaths::NormalizeDirectoryName(DirToGenGoProto);
	if (!IFileManager::Get().DirectoryExists(*DirToGenGoProto))
	{
		IFileManager::Get().MakeDirectory(*DirToGenGoProto, true);
	}
	else
	{
		// remove all files in channeld go proto storage dir
		TArray<FString> AllCodeFiles;
		IFileManager::Get().FindFiles(AllCodeFiles, *DirToGenGoProto);
		for (const FString& FilePath : AllCodeFiles)
		{
			IFileManager::Get().Delete(*(DirToGenGoProto / FilePath));
		}
	}
	FString ChanneldUnrealpbPath = ChanneldPath / TEXT("pkg") / TEXT("unrealpb");
	FPaths::NormalizeDirectoryName(ChanneldUnrealpbPath);

	const FString Args = FProtocHelper::BuildProtocProcessGoArguments(
		DirToGenGoProto,
		TEXT("paths=source_relative"),
		{
			ReplicatorStorageDir,
			ChanneldUnrealpbPath,
		},
		ProtoFiles
	);

	IFileManager& FileManager = IFileManager::Get();
	const FString ProtocPath = FProtocHelper::GetProtocPath();
	if (!FileManager.FileExists(*ProtocPath))
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Protoc path is invaild: %s"), *ProtocPath);
		return;
	}

	GenProtoGoCodeWorkThread = MakeShareable(new FChanneldProcWorkerThread(TEXT("GenerateReplicatorGoProtoThread"), ProtocPath, Args));
	GenProtoGoCodeWorkThread->ProcOutputMsgDelegate.BindUObject(GenRepNotify, &UChanneldMissionNotiProxy::ReceiveOutputMsg);
	GenProtoGoCodeWorkThread->ProcBeginDelegate.AddLambda([](FChanneldProcWorkerThread*)
		{
			UE_LOG(LogChanneldEditor, Display, TEXT("Start generating channeld go proto code..."));
		}
	);
	GenProtoGoCodeWorkThread->ProcFailedDelegate.AddLambda([](FChanneldProcWorkerThread*)
		{
			UE_LOG(LogChanneldEditor, Error, TEXT("Failed to generate channeld go proto codes!"));
		}
	);
	GenProtoGoCodeWorkThread->ProcSucceedDelegate.AddLambda([](FChanneldProcWorkerThread*)
		{
			UE_LOG(LogChanneldEditor, Display, TEXT("Successfully generated channeld go proto code."));
		}
	);
	GenProtoGoCodeWorkThread->Execute();

	IFileManager::Get().Move(*(DirToGenGoProto / TEXT("data.go")), *LatestGeneratedManifest.TemporaryGoMergeCodePath);
	IFileManager::Get().Move(*(DirToGoMain / TEXT("channeldue.gen.go")), *LatestGeneratedManifest.TemporaryGoRegistrationCodePath);
}

void FChanneldEditorModule::AddRepCompsToBPsAction()
{
	TSubclassOf<class UChanneldReplicationComponent> CompClass = GetMutableDefault<UChanneldEditorSettings>()->DefaultReplicationComponent;
	GEditor->GetEditorSubsystem<UAddCompToBPSubsystem>()->AddComponentToBlueprints(CompClass, FName(TEXT("ChanneldRepComp")));
}

void FChanneldEditorModule::OpenEditorSettingsAction()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->ShowViewer("Editor", "Plugins", "ChanneldEditorSettings");
	}
}

void FChanneldEditorModule::RecompileGameCode() const
{
#if WITH_LIVE_CODING
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	if (LiveCoding != nullptr && LiveCoding->IsEnabledByDefault())
	{
		LiveCoding->EnableForSession(true);
		if (LiveCoding->IsEnabledForSession())
		{
			LiveCoding->Compile();
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoLiveCodingCompileAfterHotReload", "Live Coding cannot be enabled while hot-reloaded modules are active. Please close the editor and build from your IDE before restarting."));
		}
		return;
	}
#endif

	// Don't allow a recompile while already compiling!
	IHotReloadInterface& HotReloadSupport = FModuleManager::LoadModuleChecked<IHotReloadInterface>(TEXT("HotReload"));
	if (!HotReloadSupport.IsCurrentlyCompiling())
	{
		// We want compiling to happen asynchronously
		HotReloadSupport.DoHotReloadFromEditor(EHotReloadFlags::None);
	}
}

#undef LOCTEXT_NAMESPACE
