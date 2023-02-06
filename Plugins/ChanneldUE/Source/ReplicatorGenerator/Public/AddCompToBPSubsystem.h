// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AddCompToBPSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class REPLICATORGENERATOR_API UAddCompToBPSubsystem : public UEditorSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	enum ProcessStatus
	{
		InActive,

		Busy,

		AddingComp,

		Canceling,

		Canceled,

		Completed,

		Failed,
	};

	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsAllowedToTick() const override final;
	virtual bool IsTickableInEditor() const;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UChanneldRepGenTickable, STATGROUP_Tickables); }

	void AddComponentToActorBlueprint(UClass* CompClass, FName CompName);

	void CancelFilterRepActor();
private:
	ProcessStatus FilterRepActorProcessStatus;
	void* FilterRepActorProcReadPipe;
	void* FilterRepActorProcWritePipe;
	uint32 FilterRepActorProcessID;
	FProcHandle FilterRepActorProcessHandle;
	FString FilterRepActorProcOutLine;
	UClass* TargetRepCompClass;
	FName RepCompName;

	TWeakPtr<SNotificationItem> FilterRepActorNotiPtr;


	void ApplyChangesToBP(AActor* ActorContex);

	void AddCompToActorBPInternal(bool ShowDialog = false);

	void SpawnRunningFilterRepActorNotification();

	void SpawnFilterRepActorSucceedNotification();

	void SpawnFilterRepActorFailedNotification();
};
