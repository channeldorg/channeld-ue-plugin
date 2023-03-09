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

	UPROPERTY(EditAnywhere)
	FString Metadata;
	
	UPROPERTY(EditAnywhere)
	int GlobalChannelFanOutIntervalMs = 50;
	
	UPROPERTY(EditAnywhere)
	int GlobalChannelFanOutDelayMs = 2000;

protected:

	virtual void InitServer() override;
	virtual void InitClient() override;
	
	// Default implementation: Destroy the Pawn related to the NetConn and close the NetConn. 
	virtual void ServerHandleClientUnsub(Channeld::ConnectionId ClientConnId, channeldpb::ChannelType ChannelType, Channeld::ChannelId ChId) override;
};
