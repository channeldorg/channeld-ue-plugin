#pragma once

#include "ChanneldTypes.h"
#include "Channeld.pb.h"
#include "google/protobuf/message.h"

class IChannelDataProvider
{
public:
	virtual channeldpb::ChannelType GetChannelType() = 0;
	virtual google::protobuf::Message* GetChannelDataTemplate() const = 0;
	virtual ChannelId GetChannelId() = 0;
	virtual void SetChannelId(ChannelId ChId) = 0;
	virtual bool IsRemoved() = 0;
	virtual void SetRemoved() = 0;
	virtual bool UpdateChannelData(google::protobuf::Message* ChannelData) = 0;
	virtual void OnChannelDataUpdated(const google::protobuf::Message* ChannelData) = 0;
	virtual ~IChannelDataProvider() {}
};
