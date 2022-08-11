// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/WorldSettings.h"
#include "ChanneldWorldSettings.generated.h"

/**
 * 
 */
UCLASS()
class CHANNELDUE_API AChanneldWorldSettings : public AWorldSettings
{
	GENERATED_BODY()
	
public:

	UPROPERTY(EditAnywhere, Category="Channeld")
	TSubclassOf<class UChannelDataView> ChannelDataViewClass;
	
};
