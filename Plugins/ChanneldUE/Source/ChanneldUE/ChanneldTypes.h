#pragma once
#include "Channeld.pb.h"

//#include "Engine/EngineBaseTypes.h"
#include "ChanneldTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogChanneld, Log, All);

typedef uint32 ConnectionId;

typedef uint32 ChannelId;

// User-space message types used in ChanneldUE
enum MessageType : uint32 {
	// Used by LowLevelSend in NetConnection/NetDriver.
	MessageType_LOW_LEVEL = 100,
	// Used by ChanneldGameInstanceSubsystem to broadcast the ProtoMessageObject from server side. The message is packed as google::protobuf::any to support anonymous types.
	MessageType_ANY = 101,
	// Used by ReplicationDriver to send/receive UE's native RPC.
	MessageType_RPC = 123,
};

/*
UENUM(BlueprintType)
enum class EChannelId : ChannelId
{
	Global = 0
};
*/
const ChannelId GlobalChannelId = 0;

const uint32 MaxPacketSize = 0x0fffff;
const uint32 MinPacketSize = 20;


UENUM(BlueprintType)
enum class EChanneldChannelType : uint8
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

UENUM(BlueprintType)
enum class EChanneldConnectionType : uint8
{
	ECoT_NO_CONNECTION = 0 UMETA(DisplayName = "NoConnetion"),
	ECoT_SERVER = 1 UMETA(DisplayName = "Server"),
	ECoT_Client = 2 UMETA(DisplayName = "Client"),
};

UENUM(BlueprintType)
enum class EChanneldBroadcastType : uint8
{
	EBT_NO_BROADCAST = 0x00 UMETA(DisplayName = "NoBroadcast"),
	EBT_ALL = 0x01 UMETA(DisplayName = "All"),
	EBT_ALL_BUT_SENDER = 0x02 UMETA(DisplayName = "AllButSender"),
	EBT_ALL_BUT_OWNER = 0x04 UMETA(DisplayName = "AllButOwner"),
	EBT_SINGLE_CONNECTION = 0x08 UMETA(DisplayName = "SingleConnetction"),
	EBT_ADJACENT_CHANNELS = 0x10 UMETA(DisplayName = "AdjacentChannels"),
};

UENUM(BlueprintType)
enum class  EChannelDataAccess : uint8
{
	EDA_NO_ACCESS = 0 UMETA(DisplayName = "NoAccess"),
	EDA_READ_ACCESS = 1 UMETA(DisplayName = "Read"),
	EDA_WRITE_ACCESS = 2 UMETA(DisplayName = "Write"),
};

USTRUCT(BlueprintType)
struct CHANNELDUE_API FChannelSubscriptionOptions
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
		EChannelDataAccess DataAccess;

	UPROPERTY(BlueprintReadWrite)
		TArray<FString> DataFieldMasks;

	UPROPERTY(BlueprintReadWrite)
		int32 FanOutIntervalMs;

	UPROPERTY(BlueprintReadWrite)
		int32 FanOutDelayMs;

	FChannelSubscriptionOptions() :
		DataAccess(EChannelDataAccess::EDA_WRITE_ACCESS),
		FanOutIntervalMs(20),
		FanOutDelayMs(0)
	{}

	void MergeFromMessage(const channeldpb::ChannelSubscriptionOptions& Target)
	{
		const google::protobuf::Reflection* MsgReflection = Target.GetReflection();
		const google::protobuf::Descriptor* MsgDescriptor = Target.GetDescriptor();

		if (MsgReflection->HasField(Target, MsgDescriptor->FindFieldByNumber(Target.kDataAccessFieldNumber)))
			DataAccess = static_cast<EChannelDataAccess>(Target.dataaccess());

		if (Target.datafieldmasks_size() > 0)
		{
			TSet<FString> DataFieldMaskSet;

			for (FString DataFieldMask : DataFieldMasks)
				DataFieldMaskSet.Add(DataFieldMask);

			for (std::string TargetDataFieldMask : Target.datafieldmasks())
				DataFieldMaskSet.Add(FString(UTF8_TO_TCHAR(TargetDataFieldMask.c_str())));

			DataFieldMasks = DataFieldMaskSet.Array();
		}

		if (MsgReflection->HasField(Target, MsgDescriptor->FindFieldByNumber(Target.kFanOutIntervalMsFieldNumber)))
			FanOutIntervalMs = Target.fanoutintervalms();

		if (MsgReflection->HasField(Target, MsgDescriptor->FindFieldByNumber(Target.kFanOutDelayMsFieldNumber)))
			FanOutDelayMs = Target.fanoutdelayms();
	}

	const TSharedPtr<channeldpb::ChannelSubscriptionOptions> ToMessage() const
	{
		auto SubOptionsMsg = MakeShared<channeldpb::ChannelSubscriptionOptions>();
		SubOptionsMsg->set_dataaccess(static_cast<channeldpb::ChannelDataAccess>(DataAccess));
		for (const FString& Mask : DataFieldMasks)
		{
			SubOptionsMsg->add_datafieldmasks(TCHAR_TO_UTF8(*Mask), Mask.Len());
		}
		SubOptionsMsg->set_fanoutintervalms(FanOutIntervalMs);
		SubOptionsMsg->set_fanoutdelayms(FanOutDelayMs);
		return SubOptionsMsg;
	}
};

USTRUCT(BlueprintType)
struct CHANNELDUE_API FSubscribedChannelInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
		int32 ConnId;

	UPROPERTY(BlueprintReadWrite)
		FChannelSubscriptionOptions SubOptions;

	UPROPERTY(BlueprintReadWrite)
		EChanneldConnectionType ConnType;

	UPROPERTY(BlueprintReadWrite)
		EChanneldChannelType ChannelType;

	void Merge(const channeldpb::SubscribedToChannelResultMessage& Target)
	{
		const google::protobuf::Reflection* MsgReflection = Target.GetReflection();
		const google::protobuf::Descriptor* MsgDescriptor = Target.GetDescriptor();

		if (MsgReflection->HasField(Target, MsgDescriptor->FindFieldByNumber(Target.kConnIdFieldNumber)))
			ConnId = Target.connid();

		if (Target.has_suboptions())
		{
			SubOptions.MergeFromMessage(Target.suboptions());
		}

		if (MsgReflection->HasField(Target, MsgDescriptor->FindFieldByNumber(Target.kConnTypeFieldNumber)))
			ConnType = static_cast<EChanneldConnectionType>(Target.conntype());

		if (MsgReflection->HasField(Target, MsgDescriptor->FindFieldByNumber(Target.kChannelTypeFieldNumber)))
			ChannelType = static_cast<EChanneldChannelType>(Target.channeltype());
	}

};

USTRUCT(BlueprintType)
struct CHANNELDUE_API FOwnedChannelInfo
{
	GENERATED_BODY()

		/*
		 * Key: Connection id of subscriber
		 * Value: Sub to channel result
		 */
		UPROPERTY(BlueprintReadWrite)
		TMap<int32, FSubscribedChannelInfo> Subscribeds;

	UPROPERTY(BlueprintReadWrite)
		EChanneldChannelType ChannelType;

	UPROPERTY(BlueprintReadWrite)
		int32 ChannelId;

	UPROPERTY(BlueprintReadWrite)
		FString Metadata;

	UPROPERTY(BlueprintReadWrite)
		int32 OwnerConnId;

};

USTRUCT(BlueprintType)
struct CHANNELDUE_API FListedChannelInfo
{
	GENERATED_BODY()

		UPROPERTY(BlueprintReadWrite)
		EChanneldChannelType ChannelType;

	UPROPERTY(BlueprintReadWrite)
		int32 ChannelId;

	UPROPERTY(BlueprintReadWrite)
		FString Metadata;
};