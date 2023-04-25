#include "ChanneldEditorSubsystem.h"

#include "ChanneldEditorSettings.h"
#include "ReplicatorGeneratorUtils.h"
#include "Commandlets/CommandletHelpers.h"
#include "ThreadUtils/FChanneldProcWorkerThread.h"
#include "ChanneldEditorTypes.h"
#include "ChanneldSettings.h"
#include "ILiveCodingModule.h"
#include "ProtocHelper.h"
#include "Developer/DesktopPlatform/Public/IDesktopPlatform.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "Async/Async.h"
#include "Misc/HotReloadInterface.h"
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

void UChanneldEditorSubsystem::UpdateReplicationCache(TFunction<void(EUpdateRepActorCacheResult Result)> PostUpdateRegActorCache, FMissionCanceled* CanceledDelegate)
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
				*FString::Printf(TEXT(" -targetplatform=WindowsServer -skipcompile -nop4 -cook -skipstage -utf8output -stdout"))
			)
		)
	);
	UpdateRepActorCacheWorkThread->ProcOutputMsgDelegate.BindUObject(UpdateRepActorCacheNotify, &UChanneldMissionNotiProxy::ReceiveOutputMsg);
	UpdateRepActorCacheWorkThread->ProcSucceedDelegate.AddLambda([this, PostUpdateRegActorCache](FChanneldProcWorkerThread*)
	{
		bUpdatingRepActorCache = false;
		PostUpdateRegActorCache(EUpdateRepActorCacheResult::URRT_Updated);
	});
	UpdateRepActorCacheWorkThread->ProcFailedDelegate.AddLambda([this, PostUpdateRegActorCache](FChanneldProcWorkerThread*)
	{
		bUpdatingRepActorCache = false;
		PostUpdateRegActorCache(EUpdateRepActorCacheResult::URRT_Failed);
	});
	if (CanceledDelegate)
	{
		CanceledDelegate->AddLambda([this]()
		{
			bUpdatingRepActorCache = false;
			if (UpdateRepActorCacheWorkThread.IsValid() && UpdateRepActorCacheWorkThread->GetThreadStatus() == EChanneldThreadStatus::Busy)
			{
				UpdateRepActorCacheWorkThread->Cancel();
			}
		});
	}

	UpdateRepActorCacheWorkThread->Execute();
}

void UChanneldEditorSubsystem::ChooseFile(FString& FilePath, bool& Success, const FString& DialogTitle, const FString& DefaultPath, const FString& FileTypes)
{
	Success = false;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	TArray<FString> OutFiles;
	if (DesktopPlatform->OpenFileDialog(GetActiveWindow(), DialogTitle, DefaultPath, TEXT(""), FileTypes, EFileDialogFlags::None, OutFiles))
	{
		Success = true;
		FilePath = OutFiles[0];
	}
}

void UChanneldEditorSubsystem::ChooseFilePathToSave(FString& FilePath, bool& Success, const FString& DialogTitle, const FString& DefaultPath, const FString& FileTypes)
{
	Success = false;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	TArray<FString> OutFiles;
	if (DesktopPlatform->SaveFileDialog(GetActiveWindow(), DialogTitle, DefaultPath, TEXT(""), FileTypes, EFileDialogFlags::None, OutFiles))
	{
		Success = true;
		FilePath = OutFiles[0];
	}
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
				UE_LOG(LogChanneldEditor, Log, TEXT("Updated the channel data message names in the channeld settings."));
				GetMutableDefault<UChanneldSettings>()->ReloadConfig();

				if (GetMutableDefault<UChanneldEditorSettings>()->bEnableCompatibleRecompilation)
				{
					UE_LOG(LogChanneldEditor, Verbose, TEXT("Auto recompile game code after generate replicator protos"));
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
						FMessageDialog::Open(EAppMsgType::OkCancel, FText::FromString(TEXT("Please close the editor and recompile the game code to make the changes take effect.")));
					});
				}

				GenRepNotify->SpawnMissionSucceedNotification(nullptr);
				bGeneratingReplication = false;
			});
		});
	});
}

void UChanneldEditorSubsystem::GenRepProtoCppCode(const TArray<FString>& ProtoFiles, TFunction<void()> PostGenRepProtoCppCodeSuccess)
{
	const FString ChanneldPath = FPlatformMisc::GetEnvironmentVariable(TEXT("CHANNELD_PATH"));
	if (ChanneldPath.IsEmpty())
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Environment variable \"CHANNELD_PATH\" is empty, please set user environment variable \"CHANNELD_PATH\" to Channeld root directory"));
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
			FailedToGenRepCode();
		}
	);
	GenRepNotify->MissionCanceled.AddLambda([this]()
	{
		if (GenProtoCppCodeWorkThread.IsValid() && GenProtoCppCodeWorkThread->GetThreadStatus() == EChanneldThreadStatus::Busy)
		{
			GenProtoCppCodeWorkThread->Cancel();
		}
	});
	GenProtoCppCodeWorkThread->ProcSucceedDelegate.AddLambda([this, ProtoFiles, ReplicatorStorageDir, PostGenRepProtoCppCodeSuccess](FChanneldProcWorkerThread*)
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

void UChanneldEditorSubsystem::GenRepProtoGoCode(const TArray<FString>& ProtoFiles, TFunction<void()> PostGenRepProtoGoCodeSuccess)
{
	const FString ChanneldPath = FPlatformMisc::GetEnvironmentVariable(TEXT("CHANNELD_PATH"));
	if (ChanneldPath.IsEmpty())
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Environment variable \"CHANNELD_PATH\" is empty, please set user environment variable \"CHANNELD_PATH\" to Channeld root directory"));
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

	FString DirToGoMain = ChanneldPath / GetMutableDefault<UChanneldEditorSettings>()->GeneratedGoReplicationCodeStorageFolder;
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

	GenProtoGoCodeWorkThread = MakeShareable(new FChanneldProcWorkerThread(TEXT("GenerateReplicatorGoProtoThread"), ProtocPath, Args));
	GenProtoGoCodeWorkThread->ProcOutputMsgDelegate.BindUObject(GenRepNotify, &UChanneldMissionNotiProxy::ReceiveOutputMsg);
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
		if (GenProtoGoCodeWorkThread.IsValid() && GenProtoGoCodeWorkThread->GetThreadStatus() == EChanneldThreadStatus::Busy)
		{
			GenProtoGoCodeWorkThread->Cancel();
		}
	});
	GenProtoGoCodeWorkThread->Execute();

	IFileManager::Get().Move(*(DirToGenGoProto / TEXT("data.go")), *LatestGeneratedManifest.TemporaryGoMergeCodePath);
	IFileManager::Get().Move(*(DirToGoMain / TEXT("channeldue.gen.go")), *LatestGeneratedManifest.TemporaryGoRegistrationCodePath);
}

void UChanneldEditorSubsystem::FailedToGenRepCode()
{
	GenRepNotify->SpawnMissionFailedNotification(nullptr);
	bGeneratingReplication = false;
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
