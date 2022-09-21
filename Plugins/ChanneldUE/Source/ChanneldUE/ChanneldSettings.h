// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChanneldSettings.generated.h"

/**
 * 
 */
UCLASS(config = ChanneldUE)
class CHANNELDUE_API UChanneldSettings : public UObject
{
	GENERATED_BODY()
	
public:
	UChanneldSettings(const FObjectInitializer& obj);
	virtual void PostInitProperties() override;

	UPROPERTY(Config, EditAnywhere, Category="View")
	TSubclassOf<class UChannelDataView> ChannelDataViewClass;

	UPROPERTY(Config, EditAnywhere)
	FString ChanneldIpForClient = "127.0.0.1";
	UPROPERTY(Config, EditAnywhere)
	int32 ChanneldPortForClient = 12108;
	UPROPERTY(Config, EditAnywhere)
	FString ChanneldIpForServer = "127.0.0.1";
	UPROPERTY(Config, EditAnywhere)
	int32 ChanneldPortForServer = 11288;

private:
	bool ParseNetAddr(const FString& Addr, FString& OutIp, int32& OutPort);
};
