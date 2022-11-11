// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "View/ChannelDataView.h"
#include "SpatialMasterServerView.generated.h"

/**
 * 
 */
UCLASS()
class CHANNELDUE_API USpatialMasterServerView : public UChannelDataView
{
	GENERATED_BODY()

public:
	USpatialMasterServerView(const FObjectInitializer& ObjectInitializer);
	
	virtual void InitServer() override;
	virtual void AddProvider(ChannelId ChId, IChannelDataProvider* Provider) override;
	virtual ChannelId GetOwningChannelId(const FNetworkGUID NetId) const override;	

	UPROPERTY(EditAnywhere)
	uint32 ClientFanOutIntervalMs = 20;
	UPROPERTY(EditAnywhere)
	uint32 ClientFanOutDelayMs = 3000;

private:

	// Maintains all the channels Master server has created.
	TSet<ChannelId> AllSpatialChannelIds;
};
