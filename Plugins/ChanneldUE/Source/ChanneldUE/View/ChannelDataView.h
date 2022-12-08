#pragma once

#include "CoreMinimal.h"
#include "ChanneldTypes.h"
#include "channeld.pb.h"
#include "ChanneldConnection.h"
#include "ChannelDataProvider.h"
#include "ChanneldNetConnection.h"
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
		UE_LOG(LogChanneld, Log, TEXT("Registered %s for channel type %d"), UTF8_TO_TCHAR(MsgTemplate->GetTypeName().c_str()), ChannelType);
	}

	UFUNCTION(BlueprintCallable)
	void RegisterChannelDataType(EChanneldChannelType ChannelType, const FString& MessageFullName);

	virtual void Initialize(UChanneldConnection* InConn);
	virtual void Unintialize();
	virtual void BeginDestroy() override;
	
	virtual bool GetSendToChannelId(UChanneldNetConnection* NetConn, uint32& OutChId) const {return false;}

	virtual void AddProvider(ChannelId ChId, IChannelDataProvider* Provider);
	virtual void AddProviderToDefaultChannel(IChannelDataProvider* Provider);
	void AddActorProvider(ChannelId ChId, AActor* Actor);
	void AddActorProvider(AActor* Actor);
	virtual void RemoveProvider(ChannelId ChId, IChannelDataProvider* Provider, bool bSendRemoved);
	virtual void RemoveProviderFromAllChannels(IChannelDataProvider* Provider, bool bSendRemoved);
	virtual void MoveProvider(ChannelId OldChId, ChannelId NewChId, IChannelDataProvider* Provider);

	virtual void OnAddClientConnection(UChanneldNetConnection* ClientConnection, ChannelId ChId){}
	virtual void OnRemoveClientConnection(UChanneldNetConnection* ClientConn){}
	virtual void OnClientPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer, UChanneldNetConnection* NewPlayerConn);
	virtual FNetworkGUID GetNetId(UObject* Obj) const;
	virtual FNetworkGUID GetNetId(IChannelDataProvider* Provider) const;
	
	/**
	 * @brief Added the object to the NetId-ChannelId mapping. If the object is an IChannelDataProvider, also add it to the providers set.\n
	 * By default, the channelId used for adding is the LowLevelSendToChannelId in the UChanneldNetDriver / UChanneldGameInstanceSubsystem.
	 * @param Obj The object just spawned on the server, passed from UChanneldNetDriver::OnServerSpawnedActor.
	 * @param NetId The NetworkGUID assigned for the object.
	 * @return Should the NetDriver send the spawn message to the clients?
	 */
	virtual bool OnServerSpawnedObject(UObject* Obj, const FNetworkGUID NetId);
	// Gives the view a chance to override the NetRole, OwningChannelId, OwningConnId, or the Location parameter.
	virtual void SendSpawnToConn(UObject* Obj, UChanneldNetConnection* NetConn, uint32 OwningConnId);
	virtual void OnClientSpawnedObject(UObject* Obj, const ChannelId ChId) {}
	virtual void OnDestroyedObject(UObject* Obj, const FNetworkGUID NetId);
	virtual void SetOwningChannelId(const FNetworkGUID NetId, ChannelId ChId);
	virtual ChannelId GetOwningChannelId(const FNetworkGUID NetId) const;
	virtual ChannelId GetOwningChannelId(const AActor* Actor) const;

	int32 SendAllChannelUpdates();

	void OnDisconnect();

	UPROPERTY(EditAnywhere)
	EChanneldChannelType DefaultChannelType = EChanneldChannelType::ECT_Global;

	// DO NOT loop-reference, otherwise the destruction can cause exception.
	// TSharedPtr< class UChanneldNetDriver > NetDriver;
	
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
	class UChanneldGameInstanceSubsystem* GetChanneldSubsystem() const;

	UObject* GetObjectFromNetGUID(const FNetworkGUID& NetId);

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
	/**
	 * @brief Server's callback when receiving the unsub message of a client.
	 * Default implementation: Destroy the Pawn related to the NetConn and close the NetConn.
	 * @param ClientConnId 
	 * @param ChId 
	 */
	virtual void OnClientUnsub(ConnectionId ClientConnId, channeldpb::ChannelType ChannelType, ChannelId ChId);
	
	void HandleChannelDataUpdate(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	
	void HandleUnsub(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);

	// Give the subclass a chance to mess with the removed providers, e.g. add a provider back to a channel.
	virtual void OnUnsubFromChannel(ChannelId ChId, const TSet<FProviderInternal>& RemovedProviders) {}

	UPROPERTY()
	UChanneldConnection* Connection;

	// Reuse the message objects to 1) decrease the memory footprint; 2) save the data for the next update if no state is consumed.
	TMap<ChannelId, google::protobuf::Message*> ReceivedUpdateDataInChannels;

	// The spawned object's NetGUID mapping to the ID of the channel that owns the object.
	TMap<const FNetworkGUID, ChannelId> NetIdOwningChannels;

private:

	TMap<channeldpb::ChannelType, const google::protobuf::Message*> ChannelDataTemplates;
	google::protobuf::Any* AnyForTypeUrl;
	TMap<FString, const google::protobuf::Message*> ChannelDataTemplatesByTypeUrl;

	TMap<ChannelId, TSet<FProviderInternal>> ChannelDataProviders;
	TMap<ChannelId, google::protobuf::Message*> RemovedProvidersData;

};
