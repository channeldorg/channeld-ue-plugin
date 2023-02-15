#pragma once

#include "CoreMinimal.h"
#include "AreaOfInterestBase.h"
#include "ChanneldNetConnection.h"
#include "ClientInterestManager.generated.h"

UCLASS(BlueprintType)
class CHANNELDUE_API UClientInterestManager : public UObject, public FTickableGameObject
{
	GENERATED_BODY()
public:
	UClientInterestManager(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	void ServerSetup(UChanneldNetConnection* ClientNetConn);
	void CleanUp(UChanneldNetConnection* ClientNetConn);

	virtual bool IsTickable() const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UClientInterestManager, STATGROUP_Tickables); }

	void AddAOI(TSharedPtr<FAreaOfInterestBase> AOI, bool bActivate = false);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Interest")
	void ActivateAOI(int Index = 0);
	
	UFUNCTION(BlueprintCallable, Category = "Channeld|Interest")
	void DeactivateAOI(int Index = 0);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Interest")
	void FollowPlayer(APlayerController* PC, int IndexOfAOI = -1);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Interest")
	void UnfollowPlayer(int IndexOfAOI = -1);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Interest")
	void AddActorInterest(AActor* Actor);
	
	UFUNCTION(BlueprintCallable, Category = "Channeld|Interest")
	void RemoveActorInterest(AActor* Actor);

private:
	TArray<TSharedPtr<FAreaOfInterestBase>> AvailableAOIs;

	TArray<TSharedPtr<FAreaOfInterestBase>> ActiveAOIs;
	
	TWeakObjectPtr<APlayerController> FollowingPC;
	FVector LastUpdateLocation;

	TArray<TWeakObjectPtr<AActor>> InterestedActors;

	void OnPlayerEnterSpatialChannel(UChanneldNetConnection* ChanneldNetConnection, Channeld::ChannelId SpatialChId);
	void OnPlayerMoved(APlayerController* PC);
};