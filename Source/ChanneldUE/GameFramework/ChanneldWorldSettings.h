#pragma once

#include "CoreMinimal.h"
#include "ChanneldTypes.h"
#include "GameFramework/WorldSettings.h"
#include "View/ChannelDataView.h"
#include "ChanneldWorldSettings.generated.h"

USTRUCT(BlueprintType, Blueprintable)
struct FServerLaunchGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "16"))
	int32 ServerNum = 1;

	// How long to wait before launching the servers (in seconds)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DelayTime = 0.f;

	// If not set, the open map in the Editor will be used.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowedClasses="World"))
	FSoftObjectPath ServerMap;

	// If not set, the ChannelDataViewClass in the UChanneldSettings will be used.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UChannelDataView> ServerViewClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText AdditionalArgs;
};

/**
 * Utility class for storing channeld-specific settings for each map.
 */
UCLASS(Config = ChanneldUE)
class CHANNELDUE_API AChanneldWorldSettings : public AWorldSettings
{
	GENERATED_BODY()
	
public:
	
	UPROPERTY(Config, EditAnywhere, Category="Channeld")
	TSubclassOf<UChannelDataView> ChannelDataViewClassOverride = nullptr;

	UPROPERTY(Config, EditAnywhere, Category="Channeld")
	TArray<FServerLaunchGroup> ServerLaunchGroupsOverride;
	
	UPROPERTY(Config, EditAnywhere, Category="Channeld")
	TArray<FString> ChanneldLaunchParametersOverride;
	
	TArray<FString> MergeLaunchParameters(const TArray<FString>& InParams) const;
};
