#pragma once

#include "CoreMinimal.h"
#include "ChanneldMissionNotiProxy.h"
#include "ChanneldEditorSubsystem.generated.h"

UENUM(BlueprintType)
enum class EUpdateRepActorCacheResult : uint8
{
	URRT_Updated,
	URRT_Updating,
	URRT_Editing,
	URRT_Failed,
};

DECLARE_DYNAMIC_DELEGATE_OneParam(FPostRepActorCache, EUpdateRepActorCacheResult, Result);

UCLASS(Meta = (DisplayName = "Channeld Editor"))
class CHANNELDEDITOR_API UChanneldEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable)
	void UpdateRepActorCacheAction(FPostRepActorCache PostUpdatedRepActorCache);

	void UpdateRepActorCache(TFunction<void(EUpdateRepActorCacheResult Result)> PostUpdateRegActorCache, FMissionCanceled* CanceledDelegate);

private:
	TSharedPtr<FChanneldProcWorkerThread> UpdateRepActorCacheWorkThread;

	UChanneldMissionNotiProxy* UpdateRepActorCacheNotify;

	bool bUpdatingRepActorCache;
};
