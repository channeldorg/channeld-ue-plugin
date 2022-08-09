#pragma once

#include "CoreMinimal.h"
#include "ChanneldTypes.h"
#include "Channeld.pb.h"
#include "ChanneldConnection.h"
#include "ChannelDataProvider.h"
#include "google/protobuf/message.h"
#include "ChannelDataView.generated.h"

UCLASS(Blueprintable, Abstract, Config=ChanneldUE)
class CHANNELDUE_API UChannelDataView : public UObject
{
	GENERATED_BODY()

public:
	UChannelDataView(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	FORCEINLINE void RegisterChannelDataTemplate(channeldpb::ChannelType ChannelType, const google::protobuf::Message* MsgTemplate)
	{
		ChannelDataTemplates.Add(ChannelType, MsgTemplate);
		if (AnyForTypeUrl == nullptr)
			AnyForTypeUrl = new google::protobuf::Any;
		AnyForTypeUrl->PackFrom(*MsgTemplate);
		ChannelDataTemplatesByTypeUrl.Add(FString(AnyForTypeUrl->type_url().c_str()), MsgTemplate);
	}

	virtual void Initialize(UChanneldConnection* InConn);
	virtual void Unintialize();

	virtual void AddProvider(ChannelId ChId, IChannelDataProvider* Provider);
	virtual void RemoveProvider(ChannelId ChId, IChannelDataProvider* Provider, bool bSendRemoved);
	//virtual void AddProviderToDefaultChannel(IChannelDataProvider* Provider);
	virtual void RemoveProviderFromAllChannels(IChannelDataProvider* Provider, bool bSendRemoved);

	void OnDisconnect();

	void SendAllChannelUpdates();

protected:

	virtual void LoadCmdLineArgs() {}

	UFUNCTION(BlueprintCallable, BlueprintPure, meta=(/*CallableWithoutWorldContext, */DisplayName = "channeld"))
	UChanneldGameInstanceSubsystem* GetSubsystem();

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "BeginPlay"))
	void ReceiveBeginPlay();

	//virtual void InitChannels() PURE_VIRTUAL(UChannelDataView::InitChannels, )

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "EndPlay"))
	void ReceiveEndPlay();

	//virtual void UninitChannels() PURE_VIRTUAL(UChannelDataView::UninitChannels, )

	virtual void OnUnsubFromChannel(ChannelId ChId, const TSet<IChannelDataProvider*>& RemovedProviders) {}

private:

	TMap<channeldpb::ChannelType, const google::protobuf::Message*> ChannelDataTemplates;
	google::protobuf::Any* AnyForTypeUrl;
	TMap<FString, const google::protobuf::Message*> ChannelDataTemplatesByTypeUrl;

	UPROPERTY()
	UChanneldConnection* Connection;

	TMap<ChannelId, TSet<IChannelDataProvider*>> ChannelDataProviders;

	void HandleUnsub(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);

	void HandleChannelDataUpdate(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);

};