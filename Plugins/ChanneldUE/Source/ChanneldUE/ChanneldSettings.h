// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChanneldTypes.h"
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
	UChanneldSettings(const FObjectInitializer& obj);
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

	UPROPERTY(Config, EditAnywhere, Category = "Replication")
	bool bSkipCustomReplication = false;
	UPROPERTY(Config, EditAnywhere, Category = "Replication")
	bool bSkipCustomRPC = true;

	UPROPERTY(Config, EditAnywhere, Category = "Spatial")
	TSubclassOf<UPlayerStartLocatorBase> PlayerStartLocatorClass;

	UPROPERTY(Config, EditAnywhere, Category = "Server")
	bool bQueueUnexportedActorRPC = false;
	
private:
	bool ParseNetAddr(const FString& Addr, FString& OutIp, int32& OutPort);
};
