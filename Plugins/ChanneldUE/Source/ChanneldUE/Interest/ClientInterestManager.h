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
	void ServerSetup(UChanneldNetConnection* InClientNetConn);
	void CleanUp();

	virtual bool IsTickable() const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UClientInterestManager, STATGROUP_Tickables); }

	void AddAOI(UAreaOfInterestBase* AOI, bool bActivate = false);
	
	UFUNCTION(BlueprintCallable, Category = "Channeld|Interest")
	UAreaOfInterestBase* GetAOIByIndex(int Index);
	
	UFUNCTION(BlueprintCallable, Category = "Channeld|Interest")
	UAreaOfInterestBase* GetAOIByName(const FName& Name);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Interest")
	void ActivateAOI(int Index = 0);
	
	UFUNCTION(BlueprintCallable, Category = "Channeld|Interest")
	void DeactivateAOI(int Index = 0);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Interest")
	void FollowActor(AActor* Target, int IndexOfAOI = -1);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Interest")
	void UnfollowActor(AActor* Target, int IndexOfAOI = -1);

private:
	
	TWeakObjectPtr<UChanneldNetConnection> ClientNetConn;

	UPROPERTY()
	TArray<UAreaOfInterestBase*> AvailableAOIs;

	UPROPERTY()
	TArray<UAreaOfInterestBase*> ActiveAOIs;

	channeldpb::SpatialInterestQuery* QueryForTick;

	// TWeakObjectPtr<APlayerController> FollowingPC;
	//
	// TArray<TWeakObjectPtr<AActor>> InterestedActors;

	void OnPlayerEnterSpatialChannel(UChanneldNetConnection* ChanneldNetConnection, Channeld::ChannelId SpatialChId);
};