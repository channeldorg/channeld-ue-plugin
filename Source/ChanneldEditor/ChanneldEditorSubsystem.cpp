#include "ChanneldEditorSubsystem.h"

#include "ReplicatorGeneratorUtils.h"
#include "Commandlets/CommandletHelpers.h"
#include "ThreadUtils/FChanneldProcWorkerThread.h"
#include "ChanneldEditorTypes.h"
#include "Developer/DesktopPlatform/Public/IDesktopPlatform.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "Async/Async.h"
#include "Windows/MinWindows.h"

#define LOCTEXT_NAMESPACE "UChanneldEditorSubsystem"

void UChanneldEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UpdateRepActorCacheNotify = NewObject<UChanneldMissionNotiProxy>();
	UpdateRepActorCacheNotify->AddToRoot();
}

void UChanneldEditorSubsystem::UpdateRepActorCacheAction(FPostRepActorCache PostUpdatedRepActorCache)
{
	UpdateRepActorCacheNotify->SetMissionNotifyText(
		FText::FromString(TEXT("Cooking And Updating Replication Actor Cache...")),
		LOCTEXT("RunningCookNotificationCancelButton", "Cancel"),
		FText::FromString(TEXT("Successfully Updated Replication Actor Cache.")),
		FText::FromString(TEXT("Failed To Update Replication Actor Cache!"))
	);
	if (!bUpdatingRepActorCache)
	{
		UpdateRepActorCacheNotify->SpawnRunningMissionNotification(nullptr);
		UpdateRepActorCache(
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

void UChanneldEditorSubsystem::UpdateRepActorCache(TFunction<void(EUpdateRepActorCacheResult Result)> PostUpdateRegActorCache, FMissionCanceled* CanceledDelegate)
{
	if (bUpdatingRepActorCache)
	{
		UE_LOG(LogChanneldEditor, Error, TEXT("Already updating replication actor cache"));
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

#undef LOCTEXT_NAMESPACE
