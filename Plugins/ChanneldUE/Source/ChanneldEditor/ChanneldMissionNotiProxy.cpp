// Fill out your copyright notice in the Description page of Project Settings.
#include "ChanneldMissionNotiProxy.h"

#include "Async/Async.h"
#include "Framework/Notifications/NotificationManager.h"
#include "ThreadUtils/FChanneldProcWorkerThread.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY_STATIC(LogMissionNotificationProxy, All, All);

#define COMMANDLET_LOG_PROXY(Type, Else) \
Else if ((MsgIndex = InMsg.Find(TEXT(#Type ":"))) != INDEX_NONE) \
{ \
	UE_LOG(LogMissionNotificationProxy, Type, TEXT("[Proxy]%s"), *InMsg.Replace(TEXT(#Type ":"), TEXT(""))); \
}

#define LOCTEXT_NAMESPACE "MissionNotificationPorxy"

UChanneldMissionNotiProxy::UChanneldMissionNotiProxy(const FObjectInitializer& Initializer): Super(Initializer)
{
}


void UChanneldMissionNotiProxy::SetMissionName(FName NewMissionName)
{
	MissionName = NewMissionName; // TEXT("Cook");
}

void UChanneldMissionNotiProxy::SetMissionNotifyText(const FText& RunningText, const FText& CancelText,
                                                     const FText& SucceedText, const FText& FailedText)
{
	// running
	RunningNotifyText = RunningText; // LOCTEXT("CookNotificationInProgress", "Cook in progress");
	// running cancel
	RunningNotifyCancelText = CancelText; // LOCTEXT("RunningCookNotificationCancelButton", "Cancel");
	// mission succeed
	MissionSucceedNotifyText = SucceedText; // LOCTEXT("CookSuccessedNotification", "Cook Finished!");
	// mission failed
	MissionFailedNotifyText = FailedText; // LOCTEXT("CookFaildNotification", "Cook Faild!");
}

FText UChanneldMissionNotiProxy::GetRunningNotifyText() const
{
	return RunningNotifyText;
}

void UChanneldMissionNotiProxy::SetRunningNotifyText(const FText& InRunningNotifyText)
{
	this->RunningNotifyText = InRunningNotifyText;
}

FText UChanneldMissionNotiProxy::GetRunningNotifyCancelText() const
{
	return RunningNotifyCancelText;
}

void UChanneldMissionNotiProxy::SetRunningNotifyCancelText(const FText& InRunningNotifyCancelText)
{
	this->RunningNotifyCancelText = InRunningNotifyCancelText;
}

FText UChanneldMissionNotiProxy::GetMissionSucceedNotifyText() const
{
	return MissionSucceedNotifyText;
}

void UChanneldMissionNotiProxy::SetMissionSucceedNotifyText(const FText& InMissionSucceedNotifyText)
{
	this->MissionSucceedNotifyText = InMissionSucceedNotifyText;
}

FText UChanneldMissionNotiProxy::GetMissionFailedNotifyText() const
{
	return MissionFailedNotifyText;
}

void UChanneldMissionNotiProxy::SetMissionFailedNotifyText(const FText& InMissionFailedNotifyText)
{
	this->MissionFailedNotifyText = InMissionFailedNotifyText;
}

FName UChanneldMissionNotiProxy::GetMissionName() const
{
	return MissionName;
}

void UChanneldMissionNotiProxy::ReceiveOutputMsg(FChanneldProcWorkerThread* Worker, const FString& InMsg)
{
	int32 MsgIndex;
	COMMANDLET_LOG_PROXY(Display,)
	COMMANDLET_LOG_PROXY(Log, else)
	COMMANDLET_LOG_PROXY(Verbose, else)
	COMMANDLET_LOG_PROXY(VeryVerbose, else)
	COMMANDLET_LOG_PROXY(Warning, else)
	COMMANDLET_LOG_PROXY(Error, else)
	COMMANDLET_LOG_PROXY(Fatal, else)
}

void UChanneldMissionNotiProxy::SpawnRunningMissionNotification(FChanneldProcWorkerThread* ProcWorker)
{
	UChanneldMissionNotiProxy* MissionProxy = this;
	AsyncTask(ENamedThreads::GameThread, [MissionProxy]()
	{
		if (MissionProxy->PendingProgressPtr.IsValid())
		{
			MissionProxy->PendingProgressPtr.Pin()->ExpireAndFadeout();
		}
		FNotificationInfo Info(MissionProxy->RunningNotifyText);

		Info.bFireAndForget = false;
		Info.Hyperlink = FSimpleDelegate::CreateStatic([]() { FGlobalTabmanager::Get()->TryInvokeTab(FName("OutputLog")); });
		Info.HyperlinkText = LOCTEXT("ShowOutputLogHyperlink", "Show Output Log");
		Info.ButtonDetails.Add(FNotificationButtonInfo(MissionProxy->RunningNotifyCancelText, FText(),
		                                               FSimpleDelegate::CreateLambda([MissionProxy]() { MissionProxy->CancelMission(); }),
		                                               SNotificationItem::CS_Pending
		));

		MissionProxy->PendingProgressPtr = FSlateNotificationManager::Get().AddNotification(Info);

		MissionProxy->PendingProgressPtr.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
		MissionProxy->bRunning = true;
	});
}

void UChanneldMissionNotiProxy::SpawnMissionSucceedNotification(FChanneldProcWorkerThread* ProcWorker)
{
	UChanneldMissionNotiProxy* MissionProxy = this;
	AsyncTask(ENamedThreads::GameThread, [MissionProxy]()
	{
		TSharedPtr<SNotificationItem> NotificationItem = MissionProxy->PendingProgressPtr.Pin();

		if (NotificationItem.IsValid())
		{
			NotificationItem->SetText(MissionProxy->MissionSucceedNotifyText);
			NotificationItem->SetCompletionState(SNotificationItem::CS_Success);
			NotificationItem->ExpireAndFadeout();

			MissionProxy->PendingProgressPtr.Reset();
		}

		MissionProxy->bRunning = false;
		UE_LOG(LogMissionNotificationProxy, Log, TEXT("The %s Mission is Successfuly."), *MissionProxy->MissionName.ToString());
	});
}

void UChanneldMissionNotiProxy::SpawnMissionFailedNotification(FChanneldProcWorkerThread* ProcWorker)
{
	UChanneldMissionNotiProxy* MissionProxy = this;
	AsyncTask(ENamedThreads::GameThread, [MissionProxy]()
	{
		TSharedPtr<SNotificationItem> NotificationItem = MissionProxy->PendingProgressPtr.Pin();

		if (NotificationItem.IsValid())
		{
			NotificationItem->SetText(MissionProxy->MissionFailedNotifyText);
			NotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
			NotificationItem->ExpireAndFadeout();

			MissionProxy->PendingProgressPtr.Reset();
			MissionProxy->bRunning = false;
			UE_LOG(LogMissionNotificationProxy, Error, TEXT("The %s Mission is faild."), *MissionProxy->MissionName.ToString())
		}
	});
}

void UChanneldMissionNotiProxy::CancelMission()
{
	MissionCanceled.Broadcast();
}

#undef LOCTEXT_NAMESPACE
