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

	UPROPERTY(EditAnywhere, Category="View")
	TSubclassOf<class UChannelDataView> ChannelDataViewClass;
	
};
