// Fill out your copyright notice in the Description page of Project Settings.


#include "SingleChannelDataView.h"
#include "ChanneldTypes.h"
#include "Channeld.pb.h"
//#include "google/protobuf/message.h"
//#include "Protobuf/ThirdParty/protobuf/include/google/protobuf/descriptor.h"

//DEFINE_LOG_CATEGORY(LogChanneld)

USingleChannelDataView::USingleChannelDataView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

void USingleChannelDataView::Initialize(UChanneldConnection* InConn)
{
	Super::Initialize(InConn);
	
	if (InConn->IsServer())
	{
		InConn->CreateChannel(channeldpb::GLOBAL, TEXT("test123"), nullptr, nullptr, nullptr,
			[&](const channeldpb::CreateChannelResultMessage* ResultMsg)
			{
				UE_LOG(LogChanneld, Log, TEXT("[%s] Created channel: %d, type: %s, owner connId: %d, metadata: %s"),
					*GetWorld()->GetDebugDisplayName(),
					ResultMsg->channelid(),
					UTF8_TO_TCHAR(channeldpb::ChannelType_Name(ResultMsg->channeltype()).c_str()),
					ResultMsg->ownerconnid(),
					UTF8_TO_TCHAR(ResultMsg->metadata().c_str()));
			});
	}
	else
	{

		InConn->SubToChannel(GlobalChannelId, nullptr, [&](const channeldpb::SubscribedToChannelResultMessage* Msg)
			{
				
				UE_LOG(LogChanneld, Log, TEXT("[%s] Sub to channel %s, connId: %d"), 
					*GetWorld()->GetDebugDisplayName(), 
					UTF8_TO_TCHAR(channeldpb::ChannelType_Name(Msg->channeltype()).c_str()),
					Msg->connid());
			});
	}
}
