#pragma once

#include "CoreMinimal.h"
#include "ChanneldTypes.h"
#include "channeld.pb.h"
#include "ChanneldConnection.h"
#include "ChannelDataProvider.h"
#include "google/protobuf/message.h"
#include "UObject/WeakInterfacePtr.h"
#include "ChannelDataView.generated.h"

// Owned by UChanneldGameInstanceSubsystem.
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
		ChannelDataTemplatesByTypeUrl.Add(FString(UTF8_TO_TCHAR(AnyForTypeUrl->type_url().c_str())), MsgTemplate);
	}

	UFUNCTION(BlueprintCallable)
	void RegisterChannelDataType(EChanneldChannelType ChannelType, const FString& MessageFullName);

	virtual void Initialize(UChanneldConnection* InConn);
	virtual void Unintialize();
	virtual void BeginDestroy() override;

	virtual void AddProvider(ChannelId ChId, IChannelDataProvider* Provider);
	virtual void RemoveProvider(ChannelId ChId, IChannelDataProvider* Provider, bool bSendRemoved);
	//virtual void AddProviderToDefaultChannel(IChannelDataProvider* Provider);
	virtual void RemoveProviderFromAllChannels(IChannelDataProvider* Provider, bool bSendRemoved);

	void OnDisconnect();

	int32 SendAllChannelUpdates();

protected:

	// TSet doesn't support TWeakInterfacePtr, so we need to wrap it in a new type. 
	struct FProviderInternal : TWeakInterfacePtr<IChannelDataProvider>
	{
		FProviderInternal(IChannelDataProvider* ProviderInstance) : TWeakInterfacePtr(ProviderInstance) {}

		bool operator==(const FProviderInternal& s) const
		{
			return Get() == s.Get();
		}

		friend FORCEINLINE uint32 GetTypeHash(const FProviderInternal& s)
		{
			return PointerHash(s.Get());
		}
	};

	virtual void LoadCmdLineArgs() {}

	UFUNCTION(BlueprintCallable, BlueprintPure/*, meta=(CallableWithoutWorldContext)*/)
	UChanneldGameInstanceSubsystem* GetChanneldSubsystem();

	virtual void InitServer();
	virtual void InitClient();

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "BeginInitServer"))
	void ReceiveInitServer();
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "BeginInitClient"))
	void ReceiveInitClient();

	virtual void UninitServer();
	virtual void UninitClient();

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "BeginUninitServer"))
	void ReceiveUninitServer();
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "BeginUninitClient"))
	void ReceiveUninitClient();

	virtual void OnUnsubFromChannel(ChannelId ChId, const TSet<FProviderInternal>& RemovedProviders) {}

	UPROPERTY()
	UChanneldConnection* Connection;

	// Reuse the message objects to 1) decrease the memory footprint; 2) save the data for the next update if no state is consumed.
	TMap<ChannelId, google::protobuf::Message*> ReceivedUpdateDataInChannels;

private:

	TMap<channeldpb::ChannelType, const google::protobuf::Message*> ChannelDataTemplates;
	google::protobuf::Any* AnyForTypeUrl;
	TMap<FString, const google::protobuf::Message*> ChannelDataTemplatesByTypeUrl;

	TMap<ChannelId, TSet<FProviderInternal>> ChannelDataProviders;
	TMap<ChannelId, google::protobuf::Message*> RemovedProvidersData;

	void HandleUnsub(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);

	void HandleChannelDataUpdate(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);

};