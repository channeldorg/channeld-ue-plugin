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

void USingleChannelDataView::InitServer()
{
	Super::InitServer();

	Connection->CreateChannel(channeldpb::GLOBAL, Metadata, nullptr, nullptr, nullptr,
		[&](const channeldpb::CreateChannelResultMessage* ResultMsg)
		{
			GetChanneldSubsystem()->SetLowLevelSendToChannelId(ResultMsg->channelid());
		});
}

void USingleChannelDataView::InitClient()
{
	Super::InitClient();

	
	channeldpb::ChannelSubscriptionOptions GlobalSubOptions;
	GlobalSubOptions.set_dataaccess(channeldpb::READ_ACCESS);
	GlobalSubOptions.set_fanoutintervalms(GlobalChannelFanOutIntervalMs);
	GlobalSubOptions.set_fanoutdelayms(GlobalChannelFanOutDelayMs);
	Connection->SubToChannel(Channeld::GlobalChannelId, &GlobalSubOptions, [&](const channeldpb::SubscribedToChannelResultMessage* ResultMsg)
	{
		GetChanneldSubsystem()->SetLowLevelSendToChannelId(Channeld::GlobalChannelId);
	});
}

void USingleChannelDataView::ServerHandleClientUnsub(Channeld::ConnectionId ClientConnId, channeldpb::ChannelType ChannelType, Channeld::ChannelId ChId)
{
	/* Moved to the base class (exception calling Destroy on the Pawn)
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
	*/
	Super::ServerHandleClientUnsub(ClientConnId, ChannelType, ChId);
}

