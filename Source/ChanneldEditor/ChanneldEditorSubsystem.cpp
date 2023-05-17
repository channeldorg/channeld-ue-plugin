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
#include "ISettingsModule.h"
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
#include "Misc/FileHelper.h"
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

	BuildServerDockerImageNotify = NewObject<UChanneldMissionNotiProxy>();
	BuildServerDockerImageNotify->AddToRoot();
	BuildServerDockerImageNotify->SetShowCancel(false);

	BuildChanneldDockerImageNotify = NewObject<UChanneldMissionNotiProxy>();
	BuildChanneldDockerImageNotify->AddToRoot();
	BuildServerDockerImageNotify->SetShowCancel(false);

	UploadDockerImageNotify = NewObject<UChanneldMissionNotiProxy>();
	UploadDockerImageNotify->AddToRoot();
	UploadDockerImageNotify->SetShowCancel(false);

	DeployToClusterNotify = NewObject<UChanneldMissionNotiProxy>();
	DeployToClusterNotify->AddToRoot();
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
		LaunchChanneldEntry;
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

void UChanneldEditorSubsystem::OpenMessageDialog(FText Message, FText OptTitle)
{
	FMessageDialog::Open(EAppMsgType::Ok, Message, &OptTitle);
}

bool UChanneldEditorSubsystem::CheckDockerCommand()
{
	return system("docker -v") == 0;
}

void UChanneldEditorSubsystem::BuildServerDockerImage(const FString& Tag,
                                                      const FPostBuildServerDockerImage& PostBuildServerDockerImage)
{
	BuildServerDockerImageNotify->SetMissionNotifyText(
		FText::FromString(TEXT("Building Server Docker Image...")),
		LOCTEXT("RunningNotificationCancelButton", "Cancel"),
		FText::FromString(TEXT("Successfully Built Server Docker Image!")),
		FText::FromString(TEXT("Failed to Build Server Docker Image!"))
	);
	BuildServerDockerImageNotify->SpawnRunningMissionNotification(nullptr);

	if (Tag.IsEmpty())
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Tag is empty!"));
		BuildServerDockerImageNotify->SpawnMissionFailedNotification(nullptr);
		PostBuildServerDockerImage.ExecuteIfBound(false);
		return;
	}

	const UProjectPackagingSettings* const PackagingSettings = GetDefault<UProjectPackagingSettings>();
	FString ServerPackagePath = PackagingSettings->StagingDirectory.Path / TEXT("LinuxServer");
	if (!FPaths::DirectoryExists(ServerPackagePath))
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Server package path is invalid: %s."), *ServerPackagePath);
		BuildServerDockerImageNotify->SpawnMissionFailedNotification(nullptr);
		PostBuildServerDockerImage.ExecuteIfBound(false);
		return;
	}

	FString DockerfileTemplate = FString(ANSI_TO_TCHAR(PLUGIN_DIR)) / TEXT("Template") / TEXT("Dockerfile-LinuxServer");
	// Load DockerfileTemplate
	FString DockerfileContent;
	{
		if (!FFileHelper::LoadFileToString(DockerfileContent, *DockerfileTemplate))
		{
			UE_LOG(LogChanneldEditor, Error, TEXT("Failed to load Dockerfile template."));
			BuildServerDockerImageNotify->SpawnMissionFailedNotification(nullptr);
			PostBuildServerDockerImage.ExecuteIfBound(false);
			return;
		}
		// Replace template args
		FStringFormatNamedArguments FormatArgs;
		FormatArgs.Add(TEXT("PackagePath"), TEXT("./LinuxServer"));
		FormatArgs.Add(TEXT("ProjectName"), FApp::GetProjectName());
		DockerfileContent = FString::Format(*DockerfileContent, FormatArgs);
	}
	// Write Dockerfile to intermediate dir
	const FString ServerDockerfilePath = FPaths::ProjectIntermediateDir() / TEXT("ChanneldClouldDeployment") / TEXT(
		"Dockerfile-LinuxServer");
	if (!FFileHelper::SaveStringToFile(DockerfileContent, *ServerDockerfilePath))
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Failed to save Dockerfile."));
		BuildServerDockerImageNotify->SpawnMissionFailedNotification(nullptr);
		PostBuildServerDockerImage.ExecuteIfBound(false);
		return;
	}

	FString BuildArgs = FString::Printf(
		TEXT("build -f \"%s\" -t %s \"%s\""), *ServerDockerfilePath, *Tag, *PackagingSettings->StagingDirectory.Path);

	FString BatTemplate = FString(ANSI_TO_TCHAR(PLUGIN_DIR)) / TEXT("Template") / TEXT("BuildDockerImage.bat");

	FString BatFileContent;
	{
		if (!FFileHelper::LoadFileToString(BatFileContent, *BatTemplate))
		{
			UE_LOG(LogChanneldEditor, Error, TEXT("Failed to load BuildServerDockerImage.bat template."));
			BuildServerDockerImageNotify->SpawnMissionFailedNotification(nullptr);
			PostBuildServerDockerImage.ExecuteIfBound(false);
			return;
		}
		FStringFormatNamedArguments FormatArgs;
		FormatArgs.Add(TEXT("BuildCmd"), TEXT("docker ") + BuildArgs);
		BatFileContent = FString::Format(*BatFileContent, FormatArgs);
	}
	// Save the cmd to a temp bat file
	const FString TempBatFilePath = FPaths::ProjectIntermediateDir() / TEXT("ChanneldClouldDeployment") / TEXT(
		"BuildServerDockerImage.bat");

	FFileHelper::SaveStringToFile(BatFileContent, *TempBatFilePath);

	AsyncTask(ENamedThreads::AnyNormalThreadNormalTask, [this, TempBatFilePath, PostBuildServerDockerImage]()
	{
		int Result = system(TCHAR_TO_ANSI(*TempBatFilePath));
		if (Result == 0)
		{
			BuildServerDockerImageNotify->SpawnMissionSucceedNotification(nullptr);
			PostBuildServerDockerImage.ExecuteIfBound(true);
		}
		else
		{
			BuildServerDockerImageNotify->SpawnMissionFailedNotification(nullptr);
			PostBuildServerDockerImage.ExecuteIfBound(false);
		}
	});
}


void UChanneldEditorSubsystem::BuildChanneldDockerImage(const FString& Tag,
                                                        const FPostBuildChanneldDockerImage&
                                                        PostBuildChanneldDockerImage)
{
	BuildChanneldDockerImageNotify->SetMissionNotifyText(
		FText::FromString(TEXT("Building Channeld Gateway...")),
		LOCTEXT("RunningNotificationCancelButton", "Cancel"),
		FText::FromString(TEXT("Successfully Built Channeld Gateway!")),
		FText::FromString(TEXT("Failed To Build Channeld Gateway!"))
	);
	BuildChanneldDockerImageNotify->SpawnRunningMissionNotification(nullptr);

	if (Tag.IsEmpty())
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Tag is empty!"));
		BuildChanneldDockerImageNotify->SpawnMissionFailedNotification(nullptr);
		PostBuildChanneldDockerImage.ExecuteIfBound(false);
		return;
	}

	FString ChanneldPath = FPlatformMisc::GetEnvironmentVariable(TEXT("CHANNELD_PATH"));
	if (ChanneldPath.IsEmpty())
	{
		UE_LOG(LogChanneldEditor, Error,
		       TEXT(
			       "CHANNELD_PATH environment variable is not set. Please set it to the path of the channeld source code directory."
		       ));
		BuildChanneldDockerImageNotify->SpawnMissionFailedNotification(nullptr);
		PostBuildChanneldDockerImage.ExecuteIfBound(false);
		return;
	}
	FPaths::NormalizeDirectoryName(ChanneldPath);

	if (!FPaths::DirectoryExists(ChanneldPath))
	{
		UE_LOG(LogChanneldEditor, Error,
		       TEXT(
			       "Channeld source code directory does not exist."
		       ));
		BuildChanneldDockerImageNotify->SpawnMissionFailedNotification(nullptr);
		PostBuildChanneldDockerImage.ExecuteIfBound(false);
		return;
	}

	FString ChanneldEntryPath = GetMutableDefault<UChanneldEditorSettings>()->LaunchChanneldEntry;
	if (ChanneldEntryPath.IsEmpty())
	{
		UE_LOG(LogChanneldEditor, Error,
		       TEXT(
			       "LaunchChanneldEntry is not set. Please set it to the path of the channeld entry point."
		       ));
		BuildChanneldDockerImageNotify->SpawnMissionFailedNotification(nullptr);
		PostBuildChanneldDockerImage.ExecuteIfBound(false);
		return;
	}
	FPaths::NormalizeDirectoryName(ChanneldEntryPath);

	if (!FPaths::DirectoryExists(ChanneldPath / ChanneldEntryPath))
	{
		UE_LOG(LogChanneldEditor, Error,
		       TEXT(
			       "Channeld entry point does not exist."
		       ));
		BuildChanneldDockerImageNotify->SpawnMissionFailedNotification(nullptr);
		PostBuildChanneldDockerImage.ExecuteIfBound(false);
		return;
	}

	const FString ChanneldDockerfilePath = ChanneldPath / ChanneldEntryPath / TEXT("Dockerfile");
	if (!FPaths::FileExists(ChanneldDockerfilePath))
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Cannot find Dockerfile at %s."), *ChanneldDockerfilePath);
		BuildChanneldDockerImageNotify->SpawnMissionFailedNotification(nullptr);
		PostBuildChanneldDockerImage.ExecuteIfBound(false);
		return;
	}

	FString BuildArgs = FString::Printf(
		TEXT("build -f \"%s\" -t %s \"%s\""), *ChanneldDockerfilePath, *Tag, *ChanneldPath);

	FString BatTemplate = FString(ANSI_TO_TCHAR(PLUGIN_DIR)) / TEXT("Template") / TEXT("BuildDockerImage.bat");

	FString BatFileContent;
	{
		if (!FFileHelper::LoadFileToString(BatFileContent, *BatTemplate))
		{
			UE_LOG(LogChanneldEditor, Error, TEXT("Failed to load BuildServerDockerImage.bat template."));
			BuildChanneldDockerImageNotify->SpawnMissionFailedNotification(nullptr);
			PostBuildChanneldDockerImage.ExecuteIfBound(false);
			return;
		}
		FStringFormatNamedArguments FormatArgs;
		FormatArgs.Add(TEXT("WorkDir"), ChanneldPath);
		FormatArgs.Add(TEXT("BuildCmd"), TEXT("docker ") + BuildArgs);
		BatFileContent = FString::Format(*BatFileContent, FormatArgs);
	}
	// Save the cmd to a temp bat file
	const FString TempBatFilePath = FPaths::ProjectIntermediateDir() / TEXT("ChanneldClouldDeployment") / TEXT(
		"BuildChanneldDockerImage.bat");

	FFileHelper::SaveStringToFile(BatFileContent, *TempBatFilePath);

	AsyncTask(ENamedThreads::AnyNormalThreadNormalTask, [this, TempBatFilePath, PostBuildChanneldDockerImage]()
	{
		int Result = system(TCHAR_TO_ANSI(*TempBatFilePath));
		if (Result == 0)
		{
			BuildChanneldDockerImageNotify->SpawnMissionSucceedNotification(nullptr);
			PostBuildChanneldDockerImage.ExecuteIfBound(true);
		}
		else
		{
			BuildChanneldDockerImageNotify->SpawnMissionFailedNotification(nullptr);
			PostBuildChanneldDockerImage.ExecuteIfBound(false);
		}
	});
}

void UChanneldEditorSubsystem::OpenPackagingSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->ShowViewer("Project", "Packaging", "Project");
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

void UChanneldEditorSubsystem::PackageProject(const FName InPlatformInfoName,
                                              const FPostPackageProject& PostPackageProject)
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
		PostPackageProject.ExecuteIfBound(false);
		return;
	}

	if (UGameMapsSettings::GetGameDefaultMap().IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
		                     LOCTEXT("MissingGameDefaultMap",
		                             "No Game Default Map specified in Project Settings > Maps & Modes."));
		PostPackageProject.ExecuteIfBound(false);
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
		PostPackageProject.ExecuteIfBound(false);
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
				PostPackageProject.ExecuteIfBound(false);
				return;
			}
		}
	}

	if (!FModuleManager::LoadModuleChecked<IProjectTargetPlatformEditorModule>("ProjectTargetPlatformEditor").
		ShowUnsupportedTargetWarning(PlatformInfo->VanillaPlatformName))
	{
		PostPackageProject.ExecuteIfBound(false);
		return;
	}

	// let the user pick a target directory
	if (PackagingSettings->StagingDirectory.Path.IsEmpty() || PackagingSettings->StagingDirectory.Path ==
		FPaths::ProjectDir())
	{
		FString OutFolderName;

		void* ParentWindowWindowHandle = nullptr;
		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
		if (MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid())
		{
			ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}

		if (!FDesktopPlatformModule::Get()->OpenDirectoryDialog(ParentWindowWindowHandle,
		                                                        LOCTEXT("PackageDirectoryDialogTitle",
		                                                                "Package project...")
		                                                        .ToString(), PackagingSettings->StagingDirectory.Path,
		                                                        OutFolderName))
		{
			PostPackageProject.ExecuteIfBound(false);
			return;
		}

		PackagingSettings->StagingDirectory.Path = OutFolderName;
		PackagingSettings->SaveConfig();
	}

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
	                                      [PostPackageProject](FString Message, double TimeSec)
	                                      {
		                                      PostPackageProject.ExecuteIfBound(Message == TEXT("Completed"));
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

void UChanneldEditorSubsystem::UploadDockerImage(const FString& ChanneldImageTag, const FString& ServerImageTag,
                                                 const FPostUploadDockerImage& PostUploadDockerImage)
{
	UploadDockerImageNotify->SetMissionNotifyText(
		FText::FromString(TEXT("Uploading Docker Image...")),
		LOCTEXT("RunningNotificationCancelButton", "Cancel"),
		FText::FromString(TEXT("Successfully Uploaded Docker Image!")),
		FText::FromString(TEXT("Failed to Upload Docker Image!"))
	);
	UploadDockerImageNotify->SpawnRunningMissionNotification(nullptr);

	if (ChanneldImageTag.IsEmpty() || ServerImageTag.IsEmpty())
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("ChanneldImageTag or ServerImageTag is empty."));
		UploadDockerImageNotify->SpawnMissionFailedNotification(nullptr);
		PostUploadDockerImage.ExecuteIfBound(false);
		return;
	}

	FString BatTemplate = FString(ANSI_TO_TCHAR(PLUGIN_DIR)) / TEXT("Template") / TEXT("UploadDockerImage.bat");

	FString BatFileContent;
	{
		if (!FFileHelper::LoadFileToString(BatFileContent, *BatTemplate))
		{
			UE_LOG(LogChanneldEditor, Error, TEXT("Failed to load BuildServerDockerImage.bat template."));
			UploadDockerImageNotify->SpawnMissionFailedNotification(nullptr);
			PostUploadDockerImage.ExecuteIfBound(false);
			return;
		}
		FStringFormatNamedArguments FormatArgs;
		FormatArgs.Add(TEXT("WorkDir"), FPaths::ProjectIntermediateDir() / TEXT("ChanneldClouldDeployment"));
		FormatArgs.Add(TEXT("ChanneldTag"), ChanneldImageTag);
		FormatArgs.Add(TEXT("ChanneldRepoUrl"), FPaths::GetPath(FPaths::GetPath(ChanneldImageTag)));
		FormatArgs.Add(TEXT("ServerTag"), ServerImageTag);
		FormatArgs.Add(TEXT("ServerRepoUrl"), FPaths::GetPath(FPaths::GetPath(ServerImageTag)));
		BatFileContent = FString::Format(*BatFileContent, FormatArgs);
	}
	// Save the cmd to a temp bat file
	const FString TempBatFilePath = FPaths::ProjectIntermediateDir() / TEXT("ChanneldClouldDeployment") / TEXT(
		"UploadDockerImage.bat");

	FFileHelper::SaveStringToFile(BatFileContent, *TempBatFilePath);

	AsyncTask(ENamedThreads::AnyNormalThreadNormalTask, [this, TempBatFilePath, PostUploadDockerImage]()
	{
		int Result = system(TCHAR_TO_ANSI(*TempBatFilePath));
		if (Result == 0)
		{
			UploadDockerImageNotify->SpawnMissionSucceedNotification(nullptr);
			PostUploadDockerImage.ExecuteIfBound(true);
		}
		else
		{
			UploadDockerImageNotify->SpawnMissionFailedNotification(nullptr);
			PostUploadDockerImage.ExecuteIfBound(false);
		}
	});
}

void UChanneldEditorSubsystem::DeployToCluster(const FDeploymentStepParams DeploymentParams,
                                               const FPostDeplymentToCluster& PostDeplymentToCluster)
{
	DeployToClusterNotify->SetMissionNotifyText(
		FText::FromString(TEXT("Deploying to Cluster...")),
		LOCTEXT("RunningNotificationCancelButton", "Cancel"),
		FText::FromString(TEXT("Successfully Deployed to Cluster!")),
		FText::FromString(TEXT("Failed to Deploy to Cluster!"))
	);
	DeployToClusterNotify->SpawnRunningMissionNotification(nullptr);

	FString Cluster = DeploymentParams.Cluster;
	FString Namespace = DeploymentParams.Namespace;
	FString ChanneldImageTag = DeploymentParams.ChanneldImageTag;
	FString ServerImageTag = DeploymentParams.ChanneldImageTag;
	FString YAMLTemplatePath = DeploymentParams.YAMLTemplatePath;


	FString YAMLTemplateContent;

	// for( int32 I = 0 ;I < DeploymentParams.ServerGroups.Num(); ++I)
	// {
	// 	FString ServerYAMLTemplateContent;
	// 	const FServerGroupForDeployment& ServerGroup  = DeploymentParams.ServerGroups[I];
	// 	if (!FFileHelper::LoadFileToString(ServerYAMLTemplateContent, *ServerGroup.YAMLTemplatePath))
	// 	{
	// 		UE_LOG(LogChanneldEditor, Error, TEXT("Failed to load YAML template at %s."), *ServerGroup.YAMLTemplatePath);
	// 		DeployToClusterNotify->SpawnMissionFailedNotification(nullptr);
	// 		PostDeplymentToCluster.ExecuteIfBound(false);
	// 		return;
	// 	}
	// 	FStringFormatNamedArguments FormatArgs;
	// 	
	// 	FormatArgs.Add(TEXT("Namespace"), Namespace);
	// 	FormatArgs.Add(TEXT("Name"), FString::Printf(TEXT("Server_%d"), I));
	// 	FormatArgs.Add(TEXT("Replicas"), ServerGroup.ServerNum);
	// 	FormatArgs.Add(TEXT("DockerImage"), ServerImageTag);
	// 	YAMLTemplateContent.Append(FString::Format(*ServerYAMLTemplateContent, FormatArgs));
	// }
	{
		FString ChanneldEntryPath = GetMutableDefault<UChanneldEditorSettings>()->LaunchChanneldEntry;
		if (ChanneldEntryPath.IsEmpty())
		{
			UE_LOG(LogChanneldEditor, Error,
			       TEXT(
				       "LaunchChanneldEntry is not set. Please set it to the path of the channeld entry point."
			       ));
			DeployToClusterNotify->SpawnMissionFailedNotification(nullptr);
			PostDeplymentToCluster.ExecuteIfBound(false);
			return;
		}
		FPaths::NormalizeDirectoryName(ChanneldEntryPath);
		FString ChanneldParams = FString::Printf(TEXT("./%s"), *FPaths::GetCleanFilename(ChanneldEntryPath));
		for (int32 I = 0; I < DeploymentParams.ChanneldParams.Num(); I++)
		{
			ChanneldParams.Append(TEXT(", "));
			ChanneldParams.Append(DeploymentParams.ChanneldParams[I]);
		}

		if (!FFileHelper::LoadFileToString(YAMLTemplateContent, *YAMLTemplatePath))
		{
			UE_LOG(LogChanneldEditor, Error, TEXT("Failed to load YAML template at %s."), *YAMLTemplatePath);
			DeployToClusterNotify->SpawnMissionFailedNotification(nullptr);
			PostDeplymentToCluster.ExecuteIfBound(false);
			return;
		}
		FStringFormatNamedArguments FormatArgs;
		FormatArgs.Add(TEXT("ChanneldParams"), ChanneldParams);
		FormatArgs.Add(TEXT("Namespace"), Namespace);
		FormatArgs.Add(TEXT("DockerImage"), ChanneldImageTag);
		// FormatArgs.Add(TEXT("ServerTag"), ServerImageTag);
		YAMLTemplateContent = FString::Format(*YAMLTemplateContent, FormatArgs);
	}
	const FString TempYAMLFilePath = FPaths::ProjectIntermediateDir() / TEXT("ChanneldClouldDeployment") / TEXT(
		"Deployment.yaml");
	FFileHelper::SaveStringToFile(YAMLTemplateContent, *TempYAMLFilePath);

	FString CheckPodStatusBatTemplate = FString(ANSI_TO_TCHAR(PLUGIN_DIR)) / TEXT("Template") / TEXT(
		"CheckPodStatus.bat");
	FString JQPath = FString(ANSI_TO_TCHAR(PLUGIN_DIR)) / TEXT("Source") / TEXT(
		"ThirdParty") / TEXT("jq-win64.exe");

	FString CheckChanneldPodStatusBatPath = FString(ANSI_TO_TCHAR(PLUGIN_DIR)) / TEXT("Template") / TEXT(
		"CheckChanneldPodStatus.bat");
	{
		FString CheckPodStatusBatFileContent;
		if (!FFileHelper::LoadFileToString(CheckPodStatusBatFileContent, *CheckPodStatusBatTemplate))
		{
			UE_LOG(LogChanneldEditor, Error, TEXT("Failed to load CheckPodStatus.bat template."));
			DeployToClusterNotify->SpawnMissionFailedNotification(nullptr);
			PostDeplymentToCluster.ExecuteIfBound(false);
			return;
		}
		FStringFormatNamedArguments FormatArgs;
		FormatArgs.Add(TEXT("WorkDir"), FPaths::ProjectIntermediateDir() / TEXT("ChanneldClouldDeployment"));
		FormatArgs.Add(TEXT("JQPath"), JQPath);
		FormatArgs.Add(
			TEXT("PodStatusJsonPath"), FPaths::ProjectIntermediateDir() / TEXT("ChanneldClouldDeployment") / TEXT(
				"ChanneldPodStatus.json"));
		FormatArgs.Add(TEXT("PodSelector"), TEXT("app=channeld-getaway"));
		FormatArgs.Add(TEXT("PodDescriptionName"), TEXT("channeld"));
		CheckPodStatusBatFileContent = FString::Format(*CheckPodStatusBatFileContent, FormatArgs);
		FFileHelper::SaveStringToFile(CheckPodStatusBatFileContent, *CheckChanneldPodStatusBatPath);
	}

	FString DeployBatTemplate = FString(ANSI_TO_TCHAR(PLUGIN_DIR)) / TEXT("Template") / TEXT("Deployment.bat");
	FString DeployBatFileContent;
	{
		if (!FFileHelper::LoadFileToString(DeployBatFileContent, *DeployBatTemplate))
		{
			UE_LOG(LogChanneldEditor, Error, TEXT("Failed to load Deployment.bat template."));
			DeployToClusterNotify->SpawnMissionFailedNotification(nullptr);
			PostDeplymentToCluster.ExecuteIfBound(false);
			return;
		}
		FStringFormatNamedArguments FormatArgs;
		FormatArgs.Add(TEXT("WorkDir"), FPaths::ProjectIntermediateDir() / TEXT("ChanneldClouldDeployment"));
		FormatArgs.Add(TEXT("YAMLFilePath"), TempYAMLFilePath);
		FormatArgs.Add(TEXT("CheckPodStatusBatPath"), CheckChanneldPodStatusBatPath);
		DeployBatFileContent = FString::Format(*DeployBatFileContent, FormatArgs);
	}

	const FString TempBatFilePath = FPaths::ProjectIntermediateDir() / TEXT("ChanneldClouldDeployment") / TEXT(
		"Deployment.bat");
	FFileHelper::SaveStringToFile(DeployBatFileContent, *TempBatFilePath);

	DeployToClusterWorkThread = MakeShareable(
		new FChanneldProcWorkerThread(
			TEXT("DeployToClusterThread"),
			TempBatFilePath,
			TEXT("")
		)
	);
	DeployToClusterWorkThread->ProcOutputMsgDelegate.BindUObject(UpdateRepActorCacheNotify,
	                                                             &UChanneldMissionNotiProxy::ReceiveOutputMsg);
	DeployToClusterNotify->MissionCanceled.AddLambda([this]()
	{
		if (DeployToClusterWorkThread.IsValid() && DeployToClusterWorkThread->GetThreadStatus() ==
			EChanneldThreadStatus::Busy)
		{
			DeployToClusterWorkThread->Cancel();
		}
	});
	DeployToClusterWorkThread->ProcSucceedDelegate.AddLambda(
		[this, PostDeplymentToCluster](FChanneldProcWorkerThread*)
		{
			DeployToClusterNotify->SpawnMissionSucceedNotification(nullptr);
			PostDeplymentToCluster.ExecuteIfBound(true);
		});
	DeployToClusterWorkThread->ProcFailedDelegate.AddLambda(
		[this, PostDeplymentToCluster](FChanneldProcWorkerThread*)
		{
			DeployToClusterNotify->SpawnMissionFailedNotification(nullptr);
			PostDeplymentToCluster.ExecuteIfBound(false);
		});
	DeployToClusterWorkThread->Execute();
	//
	// AsyncTask(ENamedThreads::AnyNormalThreadNormalTask, [this, TempBatFilePath, PostDeplymentToCluster]()
	// {
	// 	int Result = system(TCHAR_TO_ANSI(*TempBatFilePath));
	// 	if (Result == 0)
	// 	{
	// 		DeployToClusterNotify->SpawnMissionSucceedNotification(nullptr);
	// 		PostDeplymentToCluster.ExecuteIfBound(true);
	// 	}
	// 	else
	// 	{
	// 		DeployToClusterNotify->SpawnMissionFailedNotification(nullptr);
	// 		PostDeplymentToCluster.ExecuteIfBound(false);
	// 	}
	// });
}


#undef LOCTEXT_NAMESPACE
