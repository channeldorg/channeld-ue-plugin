// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChanneldTypes.h"
#include "Tools/ATintActor.h"
#include "Tools/OutlinerActor.h"
#include "View/PlayerStartLocator.h"
#include "ChanneldSettings.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, config = ChanneldUE)
class CHANNELDUE_API UChanneldSettings : public UObject
{
	GENERATED_BODY()
	
public:
	UChanneldSettings(const FObjectInitializer& ObjectInitializer);
	virtual void PostInitProperties() override;

	UPROPERTY(Config, EditAnywhere, Category="View")
	TSubclassOf<class UChannelDataView> ChannelDataViewClass;

	UPROPERTY(Config, EditAnywhere, Category = "Transport")
	FString ChanneldIpForClient = "127.0.0.1";
	UPROPERTY(Config, EditAnywhere, Category = "Transport")
	int32 ChanneldPortForClient = 12108;
	UPROPERTY(Config, EditAnywhere, Category = "Transport")
	FString ChanneldIpForServer = "127.0.0.1";
	UPROPERTY(Config, EditAnywhere, Category = "Transport")
	int32 ChanneldPortForServer = 11288;
	UPROPERTY(Config, EditAnywhere, Category = "Transport")
	bool bUseReceiveThread = false;

	UPROPERTY(Config, EditAnywhere, Category = "Transport")
	bool bDisableHandshaking = true;
	UPROPERTY(Config, EditAnywhere, Category = "Transport")
	bool bSetInternalAck = true;

	UPROPERTY(Config, EditAnywhere, Category = "Replication")
	bool bSkipCustomReplication = false;

	UPROPERTY(Config, EditAnywhere, Category = "Spatial")
	TSubclassOf<UPlayerStartLocatorBase> PlayerStartLocatorClass;
	
	UPROPERTY(Config, EditAnywhere, Category = "Spatial|Client Interest")
	float SphereRadius = 1000;
	UPROPERTY(Config, EditAnywhere, Category = "Spatial|Client Interest")
	bool bUseNetRelevantForUninterestedActors = false;
	UPROPERTY(Config, EditAnywhere, Category = "Spatial|Client Interest")
	TArray<FClientInterestSettingsPreset> ClientInterestPresets;
	
	UPROPERTY(Config, EditAnywhere, Category = "Spatial|Debug")
	bool bEnableSpatialVisualizer = false;
	UPROPERTY(Config, EditAnywhere, Category = "Spatial|Debug", meta=(EditCondition="bEnableSpatialVisualizer"))
	TSubclassOf<ATintActor> RegionBoxClass;
	UPROPERTY(Config, EditAnywhere, Category = "Spatial|Debug", meta=(EditCondition="bEnableSpatialVisualizer"))
	FVector RegionBoxMinSize;
	UPROPERTY(Config, EditAnywhere, Category = "Spatial|Debug", meta=(EditCondition="bEnableSpatialVisualizer"))
	FVector RegionBoxMaxSize;
	UPROPERTY(Config, EditAnywhere, Category = "Spatial|Debug", meta=(EditCondition="bEnableSpatialVisualizer"))
	TSubclassOf<AActor> SubscriptionBoxClass;
	UPROPERTY(Config, EditAnywhere, Category = "Spatial|Debug", meta=(EditCondition="bEnableSpatialVisualizer"))
	TSubclassOf<AOutlinerActor> SpatialOutlinerClass;

	// If set to true, the RPC with the actor that hasn't been exported to the client will be postponed until being exported.
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	bool bQueueUnexportedActorRPC = false;
	// Delay the calling of InitServer() for attaching the debugger or other purpose.
	UPROPERTY(Config, EditAnywhere, Category = "Server|Debug")
	float DelayViewInitInSeconds = 0;

private:
	bool ParseNetAddr(const FString& Addr, FString& OutIp, int32& OutPort);
};
