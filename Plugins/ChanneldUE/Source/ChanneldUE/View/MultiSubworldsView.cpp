#include "MultiSubworldsView.h"
#include "ChanneldTypes.h"
#include "Channeld.pb.h"

UMultiSubworldsView::UMultiSubworldsView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

void UMultiSubworldsView::InitServer()
{
	Connection->CreateChannel(channeldpb::SUBWORLD, ChannelMetadata, nullptr, nullptr, nullptr, nullptr);

	InitServer();
}

void UMultiSubworldsView::InitClient()
{
	Connection->ListChannel(channeldpb::SUBWORLD, nullptr, [&](const channeldpb::ListChannelResultMessage* Msg)
		{

		});

	InitClient();
}
