// Fill out your copyright notice in the Description page of Project Settings.

#include "SingleChannelDataView.h"
#include "ChanneldTypes.h"

USingleChannelDataView::USingleChannelDataView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

Channeld::ChannelId USingleChannelDataView::GetOwningChannelId(const FNetworkGUID NetId) const
{
	return Channeld::GlobalChannelId;
}

