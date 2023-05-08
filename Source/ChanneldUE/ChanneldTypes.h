#pragma once
#include "channeld.pb.h"

//#include "Engine/EngineBaseTypes.h"
#include "ChanneldTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogChanneld, Log, All);

namespace Channeld
{
	typedef uint32 ConnectionId;

	typedef uint32 ChannelId;

	typedef uint32 EntityId;

	/*
	UENUM(BlueprintType)
	enum class EChannelId : ChannelId
	{
		Global = 0
	};
	*/
	constexpr ChannelId GlobalChannelId = 0;
	constexpr ChannelId InvalidChannelId = 0xffffffff;

	constexpr uint32 GameStateNetId = 0x00080000;

	constexpr uint32 MaxPacketSize = 0x00ffff;
	constexpr uint32 MinPacketSize = 20;
	constexpr uint8 MaxConnectionIdBits = 13;
	constexpr uint8 ConnectionIdBitOffset = (31 - MaxConnectionIdBits);
}

UENUM(BlueprintType)
enum class EChanneldChannelType : uint8
{
	ECT_Unknown = 0 UMETA(DisplayName = "Unknown"),
	ECT_Global = 1 UMETA(DisplayName = "Global"),
	ECT_Private = 2 UMETA(DisplayName = "Private"),
	ECT_SubWorld = 3 UMETA(DisplayName = "Subworld"),
	ECT_Spatial = 4 UMETA(DisplayName = "Spatial"),
	ECT_Entity = 5 UMETA(DisplayName = "Entity"),

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
enum class EChannelDataAccess : uint8
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

	UPROPERTY(BlueprintReadWrite)
	bool bSkipSelfUpdateFanOut;

	UPROPERTY(BlueprintReadWrite)
	bool bSkipFirstFanOut;

	FChannelSubscriptionOptions() :
		DataAccess(EChannelDataAccess::EDA_WRITE_ACCESS),
		FanOutIntervalMs(20),
		FanOutDelayMs(0),
		bSkipSelfUpdateFanOut(true),
		bSkipFirstFanOut(false)
	{
	}

	void MergeFromMessage(const channeldpb::ChannelSubscriptionOptions& Target)
	{
		const google::protobuf::Reflection* MsgReflection = Target.GetReflection();
		const google::protobuf::Descriptor* MsgDescriptor = Target.GetDescriptor();

		if (Target.has_dataaccess())
		{
			DataAccess = static_cast<EChannelDataAccess>(Target.dataaccess());
		}

		if (Target.datafieldmasks_size() > 0)
		{
			TSet<FString> DataFieldMaskSet;

			for (FString DataFieldMask : DataFieldMasks)
			{
				DataFieldMaskSet.Add(DataFieldMask);
			}

			for (std::string TargetDataFieldMask : Target.datafieldmasks())
			{
				DataFieldMaskSet.Add(FString(UTF8_TO_TCHAR(TargetDataFieldMask.c_str())));
			}

			DataFieldMasks = DataFieldMaskSet.Array();
		}

		if (Target.has_fanoutintervalms())
		{
			FanOutIntervalMs = Target.fanoutintervalms();
		}

		if (Target.has_fanoutdelayms())
		{
			FanOutDelayMs = Target.fanoutdelayms();
		}

		if (Target.has_skipselfupdatefanout())
		{
			bSkipSelfUpdateFanOut = Target.skipselfupdatefanout();
		}

		if (Target.has_skipfirstfanout())
		{
			bSkipFirstFanOut = Target.skipfirstfanout();
		}
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
		SubOptionsMsg->set_skipselfupdatefanout(bSkipSelfUpdateFanOut);
		return SubOptionsMsg;
	}
};

USTRUCT(BlueprintType)
struct CHANNELDUE_API FSubscribedChannelInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	int32 ConnId = 0;

	UPROPERTY(BlueprintReadWrite)
	FChannelSubscriptionOptions SubOptions;

	UPROPERTY(BlueprintReadWrite)
	EChanneldConnectionType ConnType = EChanneldConnectionType::ECoT_NO_CONNECTION;

	UPROPERTY(BlueprintReadWrite)
	EChanneldChannelType ChannelType = EChanneldChannelType::ECT_Unknown;

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

	bool ShouldSendRemovalUpdate() const
	{
		return	ChannelType != EChanneldChannelType::ECT_Unknown &&
				ChannelType != EChanneldChannelType::ECT_Spatial &&
				ChannelType != EChanneldChannelType::ECT_Entity;
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
	EChanneldChannelType ChannelType = EChanneldChannelType::ECT_Unknown;

	UPROPERTY(BlueprintReadWrite)
	int32 ChannelId = 0;

	UPROPERTY(BlueprintReadWrite)
	FString Metadata;

	UPROPERTY(BlueprintReadWrite)
	int32 OwnerConnId = 0;
};

USTRUCT(BlueprintType)
struct CHANNELDUE_API FListedChannelInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	EChanneldChannelType ChannelType = EChanneldChannelType::ECT_Unknown;

	UPROPERTY(BlueprintReadWrite)
	int32 ChannelId = 0;

	UPROPERTY(BlueprintReadWrite)
	FString Metadata;
};

UENUM(BlueprintType)
enum class EClientInterestAreaType : uint8
{
	None = 0 UMETA(DisplayName = "None"),
	StaticLocations = 1 UMETA(DisplayName = "Static Locations"),
	Box = 2 UMETA(DisplayName = "Box"),
	Sphere = 3 UMETA(DisplayName = "Sphere"),
	Cone = 4 UMETA(DisplayName = "Cone"),
};

USTRUCT(BlueprintType)
struct CHANNELDUE_API FClientInterestSettingsPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Default")
	EClientInterestAreaType AreaType = EClientInterestAreaType::None;

	UPROPERTY(EditAnywhere, Category="Default")
	FName PresetName;
	
	UPROPERTY(EditAnywhere, Category="Default")
	bool bActivateByDefault = true;

	// The minimal distance required for the following actor to move before updating its client interest. If equal or less than 0, the client interest will be updated every tick.
	UPROPERTY(Config, EditAnywhere, Category = "Default")
	float MinDistanceToTriggerUpdate = 100.0f;

	// Used by SpotsAOI
	UPROPERTY(EditAnywhere, Category="Spots AOI")
	TMap<FVector, uint32> SpotsAndDists;
	// TArray<FVector> Spots;

	// Used by BoxAOI
	UPROPERTY(EditAnywhere, Category="Box AOI")
	FVector Extent = FVector(15000.0f, 15000.0f, 15000.0f);

	// Used by SphereAOI and ConeAOI
	UPROPERTY(EditAnywhere, Category="Sphere AOI")
	float Radius = 15000.0f;

	// Used by ConeAOI
	UPROPERTY(EditAnywhere, Category="Cone AOI")
	float Angle = 120.0f;
};