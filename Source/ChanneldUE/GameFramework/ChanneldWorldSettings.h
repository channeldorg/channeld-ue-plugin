#pragma once

#include "CoreMinimal.h"
#include "ChanneldTypes.h"
#include "ChanneldSettings.h"
#include "GameFramework/WorldSettings.h"
#include "View/ChannelDataView.h"
#include "Replication/ChanneldReplicationComponent.h"
#include "ChanneldWorldSettings.generated.h"

/**
 * Utility class for storing channeld-specific settings for each map.
 */
UCLASS(config=game, notplaceable)
class CHANNELDUE_API AChanneldWorldSettings : public AWorldSettings
{
	GENERATED_BODY()

public:
	AChanneldWorldSettings(const FObjectInitializer& ObjectInitializer);
	
	UPROPERTY(Config, EditAnywhere, Category="Channeld")
	TSubclassOf<UChannelDataView> ChannelDataViewClassOverride = nullptr;

	UPROPERTY(Config, EditAnywhere, Category="Channeld")
	TArray<FServerLaunchGroup> ServerLaunchGroupsOverride;
	
	UPROPERTY(Config, EditAnywhere, Category="Channeld")
	TArray<FString> ChanneldLaunchParametersOverride;
	
	TArray<FString> MergeLaunchParameters(const TArray<FString>& InParams) const;

#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category="Channeld")
	void LaunchServerInstanceInGroup0();
	
	UFUNCTION(CallInEditor, Category="Channeld")
	void LaunchServerInstanceInGroup1();
#endif

protected:
	UPROPERTY()
	UChanneldReplicationComponent* ReplicationComponent;
};
