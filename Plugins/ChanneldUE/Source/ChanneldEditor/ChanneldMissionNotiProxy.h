// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ChanneldMissionNotiProxy.generated.h"

class FChanneldProcWorkerThread;

DECLARE_MULTICAST_DELEGATE(FMissionCanceled);
/**
 * 
 */
UCLASS()
class CHANNELDEDITOR_API UChanneldMissionNotiProxy : public UObject
{
	GENERATED_UCLASS_BODY()
public:

	virtual void ReceiveOutputMsg(FChanneldProcWorkerThread* Worker,const FString& InMsg);
	virtual void SpawnRuningMissionNotification(FChanneldProcWorkerThread* ProcWorker);
	virtual void SpawnMissionSuccessedNotification(FChanneldProcWorkerThread* ProcWorker);
	virtual void SpawnMissionFaildNotification(FChanneldProcWorkerThread* ProcWorker);
	virtual void CancelMission();

	virtual void SetMissionName(FName NewMissionName);
	virtual void SetMissionNotifyText(const FText& RunningText,const FText& CancelText,const FText& SuccessedText,const FText& FaildText);
	FMissionCanceled MissionCanceled;
protected:
	TWeakPtr<SNotificationItem> PendingProgressPtr;
	bool bRunning = false;
	FText RunningNotifyText;
	FText RunningNofityCancelText;
	FText MissionSuccessedNotifyText;
	FText MissionFailedNotifyText;
	FName MissionName;
};
