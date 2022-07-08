#pragma once

//#include "Engine/EngineBaseTypes.h"

typedef uint32 ConnectionId;

typedef uint32 ChannelId;

/*
UENUM(BlueprintType)
enum class EChannelId : ChannelId
{
	Global = 0
};
*/
const ChannelId GlobalChannelId = 0;

const uint32 MaxPacketSize = 0x0fffff;