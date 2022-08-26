#pragma once

#include "CoreMinimal.h"
#include "ChanneldTypes.h"
#include "Channeld.pb.h"
#include "google/protobuf/message.h"
#include "unreal_common.pb.h"
#include "ChannelDataProvider.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UChannelDataProvider : public UInterface
{
	GENERATED_BODY()
};

class CHANNELDUE_API IChannelDataProvider
{
	GENERATED_BODY()
public:
	virtual channeldpb::ChannelType GetChannelType() = 0;
	virtual google::protobuf::Message* GetChannelDataTemplate() const = 0;
	virtual ChannelId GetChannelId() = 0;
	virtual void SetChannelId(ChannelId ChId) = 0;
	virtual bool IsRemoved() = 0;
	virtual void SetRemoved() = 0;
	virtual bool UpdateChannelData(google::protobuf::Message* ChannelData) = 0;
	virtual void OnChannelDataUpdated(google::protobuf::Message* ChannelData) = 0;
	
	static FString GetName(const IChannelDataProvider* Provider)
	{
		return Provider->_getUObject()->GetClass()->GetName();
		//return Cast<UObjectBase>(Provider)->GetClass()->GetName();
	}

};

UINTERFACE(MinimalAPI, Blueprintable)
class USceneComponentStateProvider : public UInterface
{
	GENERATED_BODY()
};

class CHANNELDUE_API ISceneComponentStateProvider
{
	GENERATED_BODY()
public:
	virtual void UpdateSceneComponent(unrealpb::SceneComponentState* State) = 0;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSceneComponentUpdated, const unrealpb::SceneComponentState*);
	FOnSceneComponentUpdated OnSceneComponentUpdated;
};