// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChannelDataView.h"
#include "SingleChannelDataView.generated.h"

class UChanneldConnection;

/**
 * 
 */
UCLASS()
class CHANNELDUE_API USingleChannelDataView : public UChannelDataView
{
	GENERATED_BODY()
	
public:
	USingleChannelDataView(const FObjectInitializer& ObjectInitializer);

	virtual Channeld::ChannelId GetOwningChannelId(const FNetworkGUID NetId) const override;
};
