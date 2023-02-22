#pragma once

#include "CoreMinimal.h"
#include "ChanneldReplicationComponent.h"
#include "FlatStatesRepComponent.generated.h"

UCLASS(BlueprintType, meta = (DisplayName = "Flat States Replication Component", BlueprintSpawnableComponent))
class CHANNELDUE_API UFlatStatesRepComponent : public UChanneldReplicationComponent
{
	GENERATED_BODY()
public:
	UFlatStatesRepComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

#if WITH_DEV_AUTOMATION_TESTS
	const google::protobuf::Message* TestGetStateFromChannelData(google::protobuf::Message* ChannelData, UClass* TargetClass, uint32 NetGUID, bool& bIsRemoved)
	{
		return GetStateFromChannelData(ChannelData, TargetClass, NetGUID, bIsRemoved);
	}
	void TestSetStateToChannelData(const google::protobuf::Message* State, google::protobuf::Message* ChannelData, UClass* TargetClass, uint32 NetGUID)
	{
		SetStateToChannelData(State, ChannelData, TargetClass, NetGUID);
	}
#endif
	
protected:
	virtual const google::protobuf::Message* GetStateFromChannelData(google::protobuf::Message* ChannelData, UClass* TargetClass, uint32 NetGUID, bool& bIsRemoved) override;
	virtual void SetStateToChannelData(const google::protobuf::Message* State, google::protobuf::Message* ChannelData, UClass* TargetClass, uint32 NetGUID) override;
};
