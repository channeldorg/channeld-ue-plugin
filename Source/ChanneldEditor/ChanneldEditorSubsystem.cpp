#include "ChanneldEditorSubsystem.h"

#include "AnalyticsEventAttribute.h"
#include "ChanneldEditorSettings.h"
#include "ReplicatorGeneratorUtils.h"
#include "Commandlets/CommandletHelpers.h"
#include "ThreadUtils/FChanneldProcWorkerThread.h"
#include "ChanneldEditorTypes.h"
#include "ChanneldSettings.h"
#include "EditorAnalytics.h"
#include "FileHelpers.h"
#include "GameMapsSettings.h"
#include "GameProjectGenerationModule.h"
#include "ILiveCodingModule.h"
#include "InstalledPlatformInfo.h"
#include "IUATHelperModule.h"
#include "PlatformInfo.h"
#include "ProtocHelper.h"
#include "SourceCodeNavigation.h"
#include "UnrealEdGlobals.h"
#include "Developer/DesktopPlatform/Public/IDesktopPlatform.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "Async/Async.h"
#include "DerivedDataCache/Public/DerivedDataCacheInterface.h"
#include "Editor/UnrealEdEngine.h"
#include "Interfaces/IMainFrameModule.h"
#include "Interfaces/IProjectTargetPlatformEditorModule.h"
#include "Logging/MessageLog.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/HotReloadInterface.h"
#include "Settings/EditorExperimentalSettings.h"
#include "Settings/ProjectPackagingSettings.h"
#include "Windows/MinWindows.h"
#include "Windows/WindowsHWrapper.h"

#define LOCTEXT_NAMESPACE "UChanneldEditorSubsystem"

void UChanneldEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UpdateRepActorCacheNotify = NewObject<UChanneldMissionNotiProxy>();
	UpdateRepActorCacheNotify->AddToRoot();

	GenRepNotify = NewObject<UChanneldMissionNotiProxy>();
	GenRepNotify->AddToRoot();
}

void UChanneldEditorSubsystem::UpdateReplicationCacheAction(FPostRepActorCache PostUpdatedRepActorCache)
{
	UpdateRepActorCacheNotify->SetMissionNotifyText(
		FText::FromString(TEXT("Cooking And Updating Replication Cache...")),
		LOCTEXT("RunningCookNotificationCancelButton", "Cancel"),
		FText::FromString(TEXT("Successfully Updated Replication Cache.")),
		FText::FromString(TEXT("Failed To Update Replication Cache!"))
	);
	if (!bUpdatingRepActorCache)
	{
		UpdateRepActorCacheNotify->SpawnRunningMissionNotification(nullptr);
		UpdateReplicationCache(
			[this, PostUpdatedRepActorCache](EUpdateRepActorCacheResult Result)
			{
				AsyncTask(ENamedThreads::GameThread, [this, Result, PostUpdatedRepActorCache]()
				{
					PostUpdatedRepActorCache.Execute(Result);
				});
				if (Result == EUpdateRepActorCacheResult::URRT_Updated)
				{
					UpdateRepActorCacheNotify->SpawnMissionSucceedNotification(nullptr);
				}
				else if (Result == EUpdateRepActorCacheResult::URRT_Updating)
				{
					return;
				}
				else
				{
					UpdateRepActorCacheNotify->SpawnMissionFailedNotification(nullptr);
				}
			}
			, &UpdateRepActorCacheNotify->MissionCanceled
		);
	}
}

void UChanneldEditorSubsystem::UpdateReplicationCache(
	TFunction<void(EUpdateRepActorCacheResult Result)> PostUpdateRegActorCache, FMissionCanceled* CanceledDelegate)
{
	if (bUpdatingRepActorCache)
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Already updating replication cache"));
		PostUpdateRegActorCache(EUpdateRepActorCacheResult::URRT_Updating);
		return;
	}
	bUpdatingRepActorCache = true;

	UpdateRepActorCacheWorkThread = MakeShareable(
		new FChanneldProcWorkerThread(
			TEXT("CookAndUpdateRepActorCacheThread"),
			ChanneldReplicatorGeneratorUtils::GetUECmdBinary(),
			CommandletHelpers::BuildCommandletProcessArguments(
				TEXT("CookAndUpdateRepActorCache"),
				*FString::Printf(TEXT("\"%s\""), *FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath())),
				*FString::Printf(
					TEXT(" -targetplatform=WindowsServer -skipcompile -nop4 -cook -skipstage -utf8output -stdout"))
			)
		)
	);
	UpdateRepActorCacheWorkThread->ProcOutputMsgDelegate.BindUObject(UpdateRepActorCacheNotify,
	                                                                 &UChanneldMissionNotiProxy::ReceiveOutputMsg);
	UpdateRepActorCacheWorkThread->ProcSucceedDelegate.AddLambda(
		[this, PostUpdateRegActorCache](FChanneldProcWorkerThread*)
		{
			bUpdatingRepActorCache = false;
			PostUpdateRegActorCache(EUpdateRepActorCacheResult::URRT_Updated);
		});
	UpdateRepActorCacheWorkThread->ProcFailedDelegate.AddLambda(
		[this, PostUpdateRegActorCache](FChanneldProcWorkerThread*)
		{
			bUpdatingRepActorCache = false;
			PostUpdateRegActorCache(EUpdateRepActorCacheResult::URRT_Failed);
		});
	if (CanceledDelegate)
	{
		CanceledDelegate->AddLambda([this]()
		{
			bUpdatingRepActorCache = false;
			if (UpdateRepActorCacheWorkThread.IsValid() && UpdateRepActorCacheWorkThread->GetThreadStatus() ==
				EChanneldThreadStatus::Busy)
			{
				UpdateRepActorCacheWorkThread->Cancel();
			}
		});
	}

	UpdateRepActorCacheWorkThread->Execute();
}

void UChanneldEditorSubsystem::ChooseFile(FString& FilePath, bool& Success, const FString& DialogTitle,
                                          const FString& DefaultPath, const FString& FileTypes)
{
	Success = false;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	TArray<FString> OutFiles;
	if (DesktopPlatform->OpenFileDialog(GetActiveWindow(), DialogTitle, DefaultPath, TEXT(""), FileTypes,
	                                    EFileDialogFlags::None, OutFiles))
	{
		Success = true;
		FilePath = OutFiles[0];
	}
}

void UChanneldEditorSubsystem::ChooseFilePathToSave(FString& FilePath, bool& Success, const FString& DialogTitle,
                                                    const FString& DefaultPath, const FString& FileTypes)
{
	Success = false;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	TArray<FString> OutFiles;
	if (DesktopPlatform->SaveFileDialog(GetActiveWindow(), DialogTitle, DefaultPath, TEXT(""), FileTypes,
	                                    EFileDialogFlags::None, OutFiles))
	{
		Success = true;
		FilePath = OutFiles[0];
	}
}

bool UChanneldEditorSubsystem::NeedToGenerateReplicationCode(bool ShowDialog /*= false*/)
{
	const FDateTime SchemaLastUpdateTime = GEditor->GetEditorSubsystem<UChannelDataSchemaController>()->
	                                                GetLastUpdatedTime();
	FGeneratedManifest LatestGeneratedManifest;
	if (!FReplicatorGeneratorManager::Get().LoadLatestGeneratedManifest(LatestGeneratedManifest))
	{
		if (ShowDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("GenerateReplicationCode",
			                                              "Replication code has not been generated yet, please generate replication code to continue"));
		}
		return true;
	}
	if (LatestGeneratedManifest.GeneratedTime < SchemaLastUpdateTime)
	{
		if (ShowDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("GenerateReplicationCode",
			                                              "Replication code is out of date, please generate replication code to continue"));
		}
		return true;
	}
	return false;
}


void UChanneldEditorSubsystem::GenerateReplicationAction()
{
	if (bGeneratingReplication)
	{
		UE_LOG(LogChanneldEditor, Warning, TEXT("Replication is already being generated"));
		return;
	}
	bGeneratingReplication = true;
	AsyncTask(ENamedThreads::GameThread, [this]()
	{
		GenRepNotify->SetMissionNotifyText(
			FText::FromString(TEXT("Generating Replication Code...")),
			LOCTEXT("RunningCookNotificationCancelButton", "Cancel"),
			FText::FromString(TEXT("Successfully Generated Replication Code.")),
			FText::FromString(TEXT("Failed To Generate Replication Code!"))
		);
		GenRepNotify->SpawnRunningMissionNotification(nullptr);
		const UChanneldEditorSettings* EditorSettings = GetMutableDefault<UChanneldEditorSettings>();

		FReplicatorGeneratorManager& GeneratorManager = FReplicatorGeneratorManager::Get();
		GeneratorManager.RemoveGeneratedCodeFiles();
		GeneratorManager.GenerateReplication(
			EditorSettings->ChanneldGoPackageImportPathPrefix,
			EditorSettings->bEnableCompatibleRecompilation
		);

		const TArray<FString> GeneratedProtoFiles = GeneratorManager.GetGeneratedProtoFiles();
		GenRepProtoCppCode(GeneratedProtoFiles, [this, GeneratedProtoFiles]()
		{
			GenRepProtoGoCode(GeneratedProtoFiles, [this]()
			{
				auto Settings = GetMutableDefault<UChanneldSettings>();
				FReplicatorGeneratorManager& GeneratorManager = FReplicatorGeneratorManager::Get();
				FGeneratedManifest LatestGeneratedManifest;
				if (!GeneratorManager.LoadLatestGeneratedManifest(LatestGeneratedManifest))
				{
					UE_LOG(LogChanneldEditor, Error, TEXT("Failed to load latest generated manifest"));
					FailedToGenRepCode();
					return;
				}
				Settings->DefaultChannelDataMsgNames = LatestGeneratedManifest.ChannelDataMsgNames;
				Settings->SaveConfig();
				UE_LOG(LogChanneldEditor, Log,
				       TEXT("Updated the channel data message names in the channeld settings."));
				GetMutableDefault<UChanneldSettings>()->ReloadConfig();

				if (GetMutableDefault<UChanneldEditorSettings>()->bEnableCompatibleRecompilation)
				{
					UE_LOG(LogChanneldEditor, Verbose,
					       TEXT("Auto recompile game code after generate replicator protos"));
					// Run RecompileGameCode in game thread, the RecompileGameCode will use FNotificationInfo which can only be used in game thread
					AsyncTask(ENamedThreads::GameThread, [this]()
					{
						RecompileGameCode();
					});
				}
				else
				{
					// Only when the dialog window is opened in the game line, the dialog window will be a ue4 editor style window
					AsyncTask(ENamedThreads::GameThread, [this]()
					{
						FMessageDialog::Open(EAppMsgType::OkCancel,
						                     FText::FromString(TEXT(
							                     "Please close the editor and recompile the game code to make the changes take effect.")));
					});
				}
				GenRepNotify->SpawnMissionSucceedNotification(nullptr);
				bGeneratingReplication = false;
				PostGenerateReplicationCode.Broadcast(true);
			});
		});
	});
}

void UChanneldEditorSubsystem::GenRepProtoCppCode(const TArray<FString>& ProtoFiles,
                                                  TFunction<void()> PostGenRepProtoCppCodeSuccess)
{
	const FString ChanneldPath = FPlatformMisc::GetEnvironmentVariable(TEXT("CHANNELD_PATH"));
	if (ChanneldPath.IsEmpty())
	{
		UE_LOG(LogChanneldEditor, Error,
		       TEXT(
			       "Environment variable \"CHANNELD_PATH\" is empty, please set user environment variable \"CHANNELD_PATH\" to Channeld root directory"
		       ));
		FailedToGenRepCode();
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
		FailedToGenRepCode();
		return;
	}

	UE_LOG(LogChanneldEditor, Display, TEXT("Generate Cpp Protobuf Code:\n\"%s\" %s"), *ProtocPath, *Args);

	GenProtoCppCodeWorkThread = MakeShareable(
		new FChanneldProcWorkerThread(TEXT("GenerateReplicatorProtoThread"), ProtocPath, Args));
	GenProtoCppCodeWorkThread->ProcOutputMsgDelegate.BindUObject(GenRepNotify,
	                                                             &UChanneldMissionNotiProxy::ReceiveOutputMsg);
	GenProtoCppCodeWorkThread->ProcBeginDelegate.AddLambda([](FChanneldProcWorkerThread*)
		{
			UE_LOG(LogChanneldEditor, Display, TEXT("Start generating cpp prototype code..."));
		}
	);
	GenProtoCppCodeWorkThread->ProcFailedDelegate.AddLambda([this](FChanneldProcWorkerThread*)
		{
			UE_LOG(LogChanneldEditor, Error, TEXT("Failed to generate cpp proto codes!"));
			FailedToGenRepCode();
		}
	);
	GenRepNotify->MissionCanceled.AddLambda([this]()
	{
		if (GenProtoCppCodeWorkThread.IsValid() && GenProtoCppCodeWorkThread->GetThreadStatus() ==
			EChanneldThreadStatus::Busy)
		{
			GenProtoCppCodeWorkThread->Cancel();
		}
	});
	GenProtoCppCodeWorkThread->ProcSucceedDelegate.AddLambda(
		[this, ProtoFiles, ReplicatorStorageDir, PostGenRepProtoCppCodeSuccess](FChanneldProcWorkerThread*)
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
			if (PostGenRepProtoCppCodeSuccess != nullptr)
			{
				PostGenRepProtoCppCodeSuccess();
			}
		}
	);

	GenProtoCppCodeWorkThread->Execute();
}

void UChanneldEditorSubsystem::GenRepProtoGoCode(const TArray<FString>& ProtoFiles,
                                                 TFunction<void()> PostGenRepProtoGoCodeSuccess)
{
	const FString ChanneldPath = FPlatformMisc::GetEnvironmentVariable(TEXT("CHANNELD_PATH"));
	if (ChanneldPath.IsEmpty())
	{
		UE_LOG(LogChanneldEditor, Error,
		       TEXT(
			       "Environment variable \"CHANNELD_PATH\" is empty, please set user environment variable \"CHANNELD_PATH\" to Channeld root directory"
		       ));
		FailedToGenRepCode();
		return;
	}
	FReplicatorGeneratorManager& GeneratorManager = FReplicatorGeneratorManager::Get();
	FGeneratedManifest LatestGeneratedManifest;
	if (!GeneratorManager.LoadLatestGeneratedManifest(LatestGeneratedManifest))
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Failed to load latest generated manifest"));
		FailedToGenRepCode();
		return;
	}

	FString DirToGoMain = ChanneldPath / GetMutableDefault<UChanneldEditorSettings>()->
		GeneratedGoReplicationCodeStorageFolder;
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
			GeneratorManager.GetReplicatorStorageDir(),
			ChanneldUnrealpbPath,
		},
		ProtoFiles
	);

	IFileManager& FileManager = IFileManager::Get();
	const FString ProtocPath = FProtocHelper::GetProtocPath();
	if (!FileManager.FileExists(*ProtocPath))
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Protoc path is invaild: %s"), *ProtocPath);
		FailedToGenRepCode();
		return;
	}

	UE_LOG(LogChanneldEditor, Display, TEXT("Generate GO Protobuf Code:\n\"%s\" %s"), *ProtocPath, *Args);

	GenProtoGoCodeWorkThread = MakeShareable(
		new FChanneldProcWorkerThread(TEXT("GenerateReplicatorGoProtoThread"), ProtocPath, Args));
	GenProtoGoCodeWorkThread->ProcOutputMsgDelegate.BindUObject(GenRepNotify,
	                                                            &UChanneldMissionNotiProxy::ReceiveOutputMsg);
	GenProtoGoCodeWorkThread->ProcBeginDelegate.AddLambda([](FChanneldProcWorkerThread*)
		{
			UE_LOG(LogChanneldEditor, Display, TEXT("Start generating channeld go proto code..."));
		}
	);
	GenProtoGoCodeWorkThread->ProcFailedDelegate.AddLambda([this](FChanneldProcWorkerThread*)
		{
			UE_LOG(LogChanneldEditor, Error, TEXT("Failed to generate channeld go proto codes!"));
			FailedToGenRepCode();
		}
	);
	GenProtoGoCodeWorkThread->ProcSucceedDelegate.AddLambda([PostGenRepProtoGoCodeSuccess](FChanneldProcWorkerThread*)
		{
			UE_LOG(LogChanneldEditor, Display, TEXT("Successfully generated channeld go proto code."));
			if (PostGenRepProtoGoCodeSuccess != nullptr)
			{
				PostGenRepProtoGoCodeSuccess();
			}
		}
	);
	GenRepNotify->MissionCanceled.AddLambda([this]()
	{
		if (GenProtoGoCodeWorkThread.IsValid() && GenProtoGoCodeWorkThread->GetThreadStatus() ==
			EChanneldThreadStatus::Busy)
		{
			GenProtoGoCodeWorkThread->Cancel();
		}
	});
	GenProtoGoCodeWorkThread->Execute();

	IFileManager::Get().Move(*(DirToGenGoProto / TEXT("data.go")), *LatestGeneratedManifest.TemporaryGoMergeCodePath);
	IFileManager::Get().Move(*(DirToGoMain / TEXT("channeldue.gen.go")),
	                         *LatestGeneratedManifest.TemporaryGoRegistrationCodePath);
}

void UChanneldEditorSubsystem::FailedToGenRepCode()
{
	GenRepNotify->SpawnMissionFailedNotification(nullptr);
	bGeneratingReplication = false;
	PostGenerateReplicationCode.Broadcast(false);
}

void UChanneldEditorSubsystem::RecompileGameCode() const
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
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoLiveCodingCompileAfterHotReload",
			                                              "Live Coding cannot be enabled while hot-reloaded modules are active. Please close the editor and build from your IDE before restarting."));
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

FString GetCookingOptionalParams()
{
	FString OptionalParams;
	const UProjectPackagingSettings* const PackagingSettings = GetDefault<UProjectPackagingSettings>();

	if (PackagingSettings->bSkipEditorContent)
	{
		OptionalParams += TEXT(" -SkipCookingEditorContent");
	}

	if (FDerivedDataCacheInterface* DDC = GetDerivedDataCache())
	{
		OptionalParams += FString::Printf(TEXT(" -ddc=%s"), DDC->GetGraphName());
	}

	return OptionalParams;
}

/**
 * Gets compilation flags for UAT for this system.
 */
const TCHAR* GetUATCompilationFlags()
{
	// We never want to compile editor targets when invoking UAT in this context.
	// If we are installed or don't have a compiler, we must assume we have a precompiled UAT.
	return TEXT("-nocompileeditor");
}

void UChanneldEditorSubsystem::PackageProject(const FName InPlatformInfoName)
{
	GUnrealEd->CancelPlayingViaLauncher();
	const bool bPromptUserToSave = false;
	const bool bSaveMapPackages = true;
	const bool bSaveContentPackages = true;
	const bool bFastSave = false;
	const bool bNotifyNoPackagesSaved = false;
	const bool bCanBeDeclined = false;
	FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages, bFastSave,
	                                    bNotifyNoPackagesSaved, bCanBeDeclined);

	// does the project have any code?
	FGameProjectGenerationModule& GameProjectModule = FModuleManager::LoadModuleChecked<FGameProjectGenerationModule>(
		TEXT("GameProjectGeneration"));
	bool bProjectHasCode = GameProjectModule.Get().ProjectHasCodeFiles();

	const PlatformInfo::FPlatformInfo* const PlatformInfo = PlatformInfo::FindPlatformInfo(InPlatformInfoName);
	check(PlatformInfo);

	if (FInstalledPlatformInfo::Get().IsPlatformMissingRequiredFile(PlatformInfo->BinaryFolderName))
	{
		if (!FInstalledPlatformInfo::OpenInstallerOptions())
		{
			FMessageDialog::Open(EAppMsgType::Ok,
			                     LOCTEXT("MissingPlatformFilesPackage",
			                             "Missing required files to package this platform."));
		}
		return;
	}

	if (UGameMapsSettings::GetGameDefaultMap().IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
		                     LOCTEXT("MissingGameDefaultMap",
		                             "No Game Default Map specified in Project Settings > Maps & Modes."));
		return;
	}

	if (PlatformInfo->SDKStatus == PlatformInfo::EPlatformSDKStatus::NotInstalled || (bProjectHasCode && PlatformInfo->
		bUsesHostCompiler && !FSourceCodeNavigation::IsCompilerAvailable()))
	{
		IMainFrameModule& MainFrameModule = FModuleManager::GetModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		MainFrameModule.BroadcastMainFrameSDKNotInstalled(PlatformInfo->TargetPlatformName.ToString(),
		                                                  PlatformInfo->SDKTutorial);
		TArray<FAnalyticsEventAttribute> ParamArray;
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("Time"), 0.0));
		FEditorAnalytics::ReportEvent(TEXT("Editor.Package.Failed"), PlatformInfo->TargetPlatformName.ToString(),
		                              bProjectHasCode, EAnalyticsErrorCodes::SDKNotFound, ParamArray);
		return;
	}

	UProjectPackagingSettings* PackagingSettings = Cast<UProjectPackagingSettings>(
		UProjectPackagingSettings::StaticClass()->GetDefaultObject());
	const UProjectPackagingSettings::FConfigurationInfo& ConfigurationInfo =
		UProjectPackagingSettings::ConfigurationInfo[PackagingSettings->BuildConfiguration];
	bool bAssetNativizationEnabled = (PackagingSettings->BlueprintNativizationMethod !=
		EProjectPackagingBlueprintNativizationMethod::Disabled);

	const ITargetPlatform* const Platform = GetTargetPlatformManager()->FindTargetPlatform(
		PlatformInfo->TargetPlatformName.ToString());
	{
		if (Platform)
		{
			FString NotInstalledTutorialLink;
			FString DocumentationLink;
			FText CustomizedLogMessage;

			int32 Result = Platform->CheckRequirements(bProjectHasCode, ConfigurationInfo.Configuration,
			                                           bAssetNativizationEnabled, NotInstalledTutorialLink,
			                                           DocumentationLink, CustomizedLogMessage);

			// report to analytics
			FEditorAnalytics::ReportBuildRequirementsFailure(
				TEXT("Editor.Package.Failed"), PlatformInfo->TargetPlatformName.ToString(), bProjectHasCode, Result);

			// report to main frame
			bool UnrecoverableError = false;

			// report to message log
			if ((Result & ETargetPlatformReadyStatus::SDKNotFound) != 0)
			{
				AddMessageLog(
					LOCTEXT("SdkNotFoundMessage", "Software Development Kit (SDK) not found."),
					CustomizedLogMessage.IsEmpty()
						? FText::Format(LOCTEXT("SdkNotFoundMessageDetail",
						                        "Please install the SDK for the {0} target platform!"), Platform->
						                DisplayName())
						: CustomizedLogMessage,
					NotInstalledTutorialLink,
					DocumentationLink
				);
				UnrecoverableError = true;
			}

			if ((Result & ETargetPlatformReadyStatus::LicenseNotAccepted) != 0)
			{
				AddMessageLog(
					LOCTEXT("LicenseNotAcceptedMessage", "License not accepted."),
					CustomizedLogMessage.IsEmpty()
						? LOCTEXT("LicenseNotAcceptedMessageDetail",
						          "License must be accepted in project settings to deploy your app to the device.")
						: CustomizedLogMessage,
					NotInstalledTutorialLink,
					DocumentationLink
				);

				UnrecoverableError = true;
			}

			if ((Result & ETargetPlatformReadyStatus::ProvisionNotFound) != 0)
			{
				AddMessageLog(
					LOCTEXT("ProvisionNotFoundMessage", "Provision not found."),
					CustomizedLogMessage.IsEmpty()
						? LOCTEXT("ProvisionNotFoundMessageDetail",
						          "A provision is required for deploying your app to the device.")
						: CustomizedLogMessage,
					NotInstalledTutorialLink,
					DocumentationLink
				);
				UnrecoverableError = true;
			}

			if ((Result & ETargetPlatformReadyStatus::SigningKeyNotFound) != 0)
			{
				AddMessageLog(
					LOCTEXT("SigningKeyNotFoundMessage", "Signing key not found."),
					CustomizedLogMessage.IsEmpty()
						? LOCTEXT("SigningKeyNotFoundMessageDetail",
						          "The app could not be digitally signed, because the signing key is not configured.")
						: CustomizedLogMessage,
					NotInstalledTutorialLink,
					DocumentationLink
				);
				UnrecoverableError = true;
			}

			if ((Result & ETargetPlatformReadyStatus::ManifestNotFound) != 0)
			{
				AddMessageLog(
					LOCTEXT("ManifestNotFound", "Manifest not found."),
					CustomizedLogMessage.IsEmpty()
						? LOCTEXT("ManifestNotFoundMessageDetail",
						          "The generated application manifest could not be found.")
						: CustomizedLogMessage,
					NotInstalledTutorialLink,
					DocumentationLink
				);
				UnrecoverableError = true;
			}

			if ((Result & ETargetPlatformReadyStatus::RemoveServerNameEmpty) != 0
				&& (bProjectHasCode || (Result & ETargetPlatformReadyStatus::CodeBuildRequired)
					|| (!FApp::GetEngineIsPromotedBuild() && !FApp::IsEngineInstalled())))
			{
				AddMessageLog(
					LOCTEXT("RemoveServerNameNotFound", "Remote compiling requires a server name. "),
					CustomizedLogMessage.IsEmpty()
						? LOCTEXT("RemoveServerNameNotFoundDetail",
						          "Please specify one in the Remote Server Name settings field.")
						: CustomizedLogMessage,
					NotInstalledTutorialLink,
					DocumentationLink
				);
				UnrecoverableError = true;
			}

			if ((Result & ETargetPlatformReadyStatus::CodeUnsupported) != 0)
			{
				FMessageDialog::Open(EAppMsgType::Ok,
				                     LOCTEXT("NotSupported_SelectedPlatform",
				                             "Sorry, packaging a code-based project for the selected platform is currently not supported. This feature may be available in a future release."));
				UnrecoverableError = true;
			}
			else if ((Result & ETargetPlatformReadyStatus::PluginsUnsupported) != 0)
			{
				FMessageDialog::Open(EAppMsgType::Ok,
				                     LOCTEXT("NotSupported_ThirdPartyPlugins",
				                             "Sorry, packaging a project with third-party plugins is currently not supported for the selected platform. This feature may be available in a future release."));
				UnrecoverableError = true;
			}

			if (UnrecoverableError)
			{
				return;
			}
		}
	}

	if (!FModuleManager::LoadModuleChecked<IProjectTargetPlatformEditorModule>("ProjectTargetPlatformEditor").
		ShowUnsupportedTargetWarning(PlatformInfo->VanillaPlatformName))
	{
		return;
	}

	// let the user pick a target directory
	if (PackagingSettings->StagingDirectory.Path.IsEmpty())
	{
		PackagingSettings->StagingDirectory.Path = FPaths::ProjectDir();
	}

	FString OutFolderName;

	void* ParentWindowWindowHandle = nullptr;
	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
	if (MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid())
	{
		ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
	}

	if (!FDesktopPlatformModule::Get()->OpenDirectoryDialog(ParentWindowWindowHandle,
	                                                        LOCTEXT("PackageDirectoryDialogTitle", "Package project...")
	                                                        .ToString(), PackagingSettings->StagingDirectory.Path,
	                                                        OutFolderName))
	{
		return;
	}

	PackagingSettings->StagingDirectory.Path = OutFolderName;
	PackagingSettings->SaveConfig();

	// create the packager process
	FString OptionalParams;

	if (PackagingSettings->FullRebuild)
	{
		OptionalParams += TEXT(" -clean");
	}

	if (PackagingSettings->bCompressed)
	{
		OptionalParams += TEXT(" -compressed");
	}

	OptionalParams += GetCookingOptionalParams();

	if (PackagingSettings->bUseIoStore)
	{
		OptionalParams += TEXT(" -iostore");

		// Pak file(s) must be used when using container file(s)
		PackagingSettings->UsePakFile = true;
	}

	if (PackagingSettings->UsePakFile)
	{
		OptionalParams += TEXT(" -pak");
	}

	if (PackagingSettings->bUseIoStore)
	{
		OptionalParams += TEXT(" -iostore");
	}

	if (PackagingSettings->bMakeBinaryConfig)
	{
		OptionalParams += TEXT(" -makebinaryconfig");
	}

	if (PackagingSettings->IncludePrerequisites)
	{
		OptionalParams += TEXT(" -prereqs");
	}

	if (!PackagingSettings->ApplocalPrerequisitesDirectory.Path.IsEmpty())
	{
		OptionalParams += FString::Printf(
			TEXT(" -applocaldirectory=\"%s\""), *(PackagingSettings->ApplocalPrerequisitesDirectory.Path));
	}
	else if (PackagingSettings->IncludeAppLocalPrerequisites)
	{
		OptionalParams += TEXT(" -applocaldirectory=\"$(EngineDir)/Binaries/ThirdParty/AppLocalDependencies\"");
	}

	if (PackagingSettings->ForDistribution)
	{
		OptionalParams += TEXT(" -distribution");
	}

	if (!PackagingSettings->IncludeDebugFiles)
	{
		OptionalParams += TEXT(" -nodebuginfo");
	}

	if (PackagingSettings->bGenerateChunks)
	{
		OptionalParams += TEXT(" -manifests");
	}

	bool bTargetPlatformCanUseCrashReporter = PlatformInfo->bTargetPlatformCanUseCrashReporter;
	if (bTargetPlatformCanUseCrashReporter && PlatformInfo->TargetPlatformName == FName("WindowsNoEditor") &&
		PlatformInfo->PlatformFlavor == TEXT("Win32"))
	{
		FString MinumumSupportedWindowsOS;
		GConfig->GetString(
			TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("MinimumOSVersion"),
			MinumumSupportedWindowsOS, GEngineIni);
		if (MinumumSupportedWindowsOS == TEXT("MSOS_XP"))
		{
			OptionalParams += TEXT(" -SpecifiedArchitecture=_xp");
			bTargetPlatformCanUseCrashReporter = false;
		}
	}

	// Append any extra UAT flags specified for this platform flavor
	if (!PlatformInfo->UATCommandLine.IsEmpty())
	{
		OptionalParams += TEXT(" ");
		OptionalParams += PlatformInfo->UATCommandLine;
	}
	else
	{
		OptionalParams += TEXT(" -targetplatform=");
		OptionalParams += *PlatformInfo->TargetPlatformName.ToString();
	}

	// Get the target to build
	const FTargetInfo* Target = PackagingSettings->GetBuildTargetInfo();

	// Only build if the user elects to do so
	bool bBuild = false;
	if (PackagingSettings->Build == EProjectPackagingBuild::Always)
	{
		bBuild = true;
	}
	else if (PackagingSettings->Build == EProjectPackagingBuild::Never)
	{
		bBuild = false;
	}
	else if (PackagingSettings->Build == EProjectPackagingBuild::IfProjectHasCode)
	{
		bBuild = true;
		if (FApp::GetEngineIsPromotedBuild() && !bAssetNativizationEnabled)
		{
			FString BaseDir;

			// Get the target name
			FString TargetName;
			if (Target == nullptr)
			{
				TargetName = TEXT("UE4Game");
			}
			else
			{
				TargetName = Target->Name;
			}

			// Get the directory containing the receipt for this target, depending on whether the project needs to be built or not
			FString ProjectDir = FPaths::GetPath(FPaths::GetProjectFilePath());
			if (Target != nullptr && FPaths::IsUnderDirectory(Target->Path, ProjectDir))
			{
				UE_LOG(LogChanneldEditor, Log, TEXT("Selected target: %s"), *Target->Name);
				BaseDir = ProjectDir;
			}
			else
			{
				FText Reason;
				if (Platform->RequiresTempTarget(bProjectHasCode, ConfigurationInfo.Configuration, false, Reason))
				{
					UE_LOG(LogChanneldEditor, Log, TEXT("Project requires temp target (%s)"), *Reason.ToString());
					BaseDir = ProjectDir;
				}
				else
				{
					UE_LOG(LogChanneldEditor, Log, TEXT("Project does not require temp target"));
					BaseDir = FPaths::EngineDir();
				}
			}

			// Check if the receipt is for a matching promoted target
			FString PlatformName = Platform->GetPlatformInfo().UBTTargetId.ToString();
		}
	}
	else if (PackagingSettings->Build == EProjectPackagingBuild::IfEditorWasBuiltLocally)
	{
		bBuild = !FApp::GetEngineIsPromotedBuild();
	}
	if (bBuild)
	{
		OptionalParams += TEXT(" -build");
	}

	// Whether to include the crash reporter.
	if (PackagingSettings->IncludeCrashReporter && bTargetPlatformCanUseCrashReporter)
	{
		OptionalParams += TEXT(" -CrashReporter");
	}

	if (PackagingSettings->bBuildHttpChunkInstallData)
	{
		OptionalParams += FString::Printf(
			TEXT(" -manifests -createchunkinstall -chunkinstalldirectory=\"%s\" -chunkinstallversion=%s"),
			*(PackagingSettings->HttpChunkInstallDataDirectory.Path),
			*(PackagingSettings->HttpChunkInstallDataVersion));
	}

	int32 NumCookers = GetDefault<UEditorExperimentalSettings>()->MultiProcessCooking;
	if (NumCookers > 0)
	{
		OptionalParams += FString::Printf(TEXT(" -NumCookersToSpawn=%d"), NumCookers);
	}

	if (Target == nullptr)
	{
		OptionalParams += FString::Printf(TEXT(" -clientconfig=%s"), LexToString(ConfigurationInfo.Configuration));
	}
	else if (Target->Type == EBuildTargetType::Server)
	{
		OptionalParams += FString::Printf(
			TEXT(" -target=%s -serverconfig=%s"), *Target->Name, LexToString(ConfigurationInfo.Configuration));
	}
	else
	{
		OptionalParams += FString::Printf(
			TEXT(" -target=%s -clientconfig=%s"), *Target->Name, LexToString(ConfigurationInfo.Configuration));
	}

	FString ProjectPath = FPaths::IsProjectFilePathSet()
		                      ? FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath())
		                      : FPaths::RootDir() / FApp::GetProjectName() / FApp::GetProjectName() + TEXT(".uproject");
	FString CommandLine = FString::Printf(
		TEXT(
			"-ScriptsForProject=\"%s\" BuildCookRun %s%s -nop4 -project=\"%s\" -cook -stage -archive -archivedirectory=\"%s\" -package -ue4exe=\"%s\" %s -utf8output"),
		*ProjectPath,
		GetUATCompilationFlags(),
		FApp::IsEngineInstalled() ? TEXT(" -installed") : TEXT(""),
		*ProjectPath,
		*PackagingSettings->StagingDirectory.Path,
		*FUnrealEdMisc::Get().GetExecutableForCommandlets(),
		*OptionalParams
	);

	IUATHelperModule::Get().CreateUatTask(CommandLine, PlatformInfo->DisplayName,
	                                      LOCTEXT("PackagingProjectTaskName", "Packaging project"),
	                                      LOCTEXT("PackagingTaskName", "Packaging"),
	                                      FEditorStyle::GetBrush(TEXT("MainFrame.PackageProject")),
	                                      [](FString Message, double TimeSec)
	                                      {
	                                      	if(Message == TEXT("Completed"))
	                                      	{
	                                      		FMessageLog("PackagingResults").Open(EMessageSeverity::Info, true);
	                                      	}
	                                      	else
	                                      	{
	                                      		FMessageLog("PackagingResults").Info(FText::FromString(Message));
	                                      	}
	                                      }
	);
}

void UChanneldEditorSubsystem::AddMessageLog(const FText& Text, const FText& Detail, const FString& TutorialLink,
                                             const FString& DocumentationLink)
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
	Message->AddToken(FTextToken::Create(Text));
	Message->AddToken(FTextToken::Create(Detail));
	if (!TutorialLink.IsEmpty())
	{
		Message->AddToken(FTutorialToken::Create(TutorialLink));
	}
	if (!DocumentationLink.IsEmpty())
	{
		Message->AddToken(FDocumentationToken::Create(DocumentationLink));
	}
	FMessageLog MessageLog("PackagingResults");
	MessageLog.AddMessage(Message);
	MessageLog.Open();
}

#undef LOCTEXT_NAMESPACE
