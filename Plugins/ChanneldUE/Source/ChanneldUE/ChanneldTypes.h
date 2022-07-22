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


UENUM(BlueprintType)
enum class ECDChannelType : uint8
{
	ECT_Unknown = 0 UMETA(DisplayName = "Unknown"),
	ECT_Global = 1 UMETA(DisplayName = "Global"),
	ECT_Private = 2 UMETA(DisplayName = "Private"),
	ECT_SubWorld = 3 UMETA(DisplayName = "Subworld"),
	ECT_Spatial = 4 UMETA(DisplayName = "Spatial"),

	/* Customized ChannelType */
	ECT_CDChannelType1 = 100 UMETA(Hidden),
	ECT_CDChannelType2 = 101 UMETA(Hidden),
	ECT_CDChannelType3 = 102 UMETA(Hidden),
	ECT_CDChannelType4 = 103 UMETA(Hidden),
	ECT_CDChannelType5 = 104 UMETA(Hidden),
	ECT_CDChannelType6 = 105 UMETA(Hidden),
	ECT_CDChannelType7 = 106 UMETA(Hidden),
	ECT_CDChannelType8 = 107 UMETA(Hidden),
	ECT_CDChannelType9 = 108 UMETA(Hidden),
	ECT_CDChannelType10 = 109 UMETA(Hidden),

	ECT_Max UMETA(Hidden),
};
