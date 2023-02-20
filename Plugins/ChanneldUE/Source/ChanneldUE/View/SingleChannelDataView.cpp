// Fill out your copyright notice in the Description page of Project Settings.

#include "SingleChannelDataView.h"

#include "ChanneldGameInstanceSubsystem.h"
#include "ChanneldNetDriver.h"
#include "ChanneldTypes.h"

USingleChannelDataView::USingleChannelDataView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

Channeld::ChannelId USingleChannelDataView::GetOwningChannelId(const FNetworkGUID NetId) const
{
	return Channeld::GlobalChannelId;
}

void USingleChannelDataView::ServerHandleClientUnsub(Channeld::ConnectionId ClientConnId, channeldpb::ChannelType ChannelType, Channeld::ChannelId ChId)
{
	if (auto NetDriver = GetChanneldSubsystem()->GetNetDriver())
	{
		UChanneldNetConnection* ClientConn;
		if (NetDriver->GetClientConnectionMap().RemoveAndCopyValue(ClientConnId, ClientConn))
		{
			//~ Start copy from UNetDriver::Shutdown()
			if (ClientConn->PlayerController)
			{
				APawn* Pawn = ClientConn->PlayerController->GetPawn();
				if (Pawn)
				{
					Pawn->Destroy(true);
				}
			}

			// Calls Close() internally and removes from ClientConnections
			// Will also destroy the player controller.
			ClientConn->CleanUp();
			//~ End copy
		}
	}
}

