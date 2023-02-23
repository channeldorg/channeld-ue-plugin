#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"
#include "google/protobuf/message.h"
#include "ChanneldTypes.h"
#include "channeld.pb.h"
#include "ChanneldConnection.generated.h"

class UChanneldConnection;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FChanneldMessageDelegate, UChanneldConnection*, Channeld::ChannelId, const google::protobuf::Message*)
DECLARE_MULTICAST_DELEGATE_FourParams(FUserSpaceMessageDelegate, uint32, Channeld::ChannelId, Channeld::ConnectionId, const std::string&)
DECLARE_MULTICAST_DELEGATE_OneParam(FChanneldAuthenticatedDelegate, UChanneldConnection*);

typedef TFunction<void(UChanneldConnection*, Channeld::ChannelId, const google::protobuf::Message*)> FChanneldMessageHandlerFunc;
//typedef TFunction<void(Channeld::ChannelId, ConnectionId, const std::string&)> FUserSpaceMessageHandlerFunc;

UCLASS(transient, config = ChanneldUE)
class CHANNELDUE_API UChanneldConnection : public UEngineSubsystem, public FRunnable
{
	GENERATED_BODY()

public:

	//UChanneldConnection(const FObjectInitializer& ObjectInitializer);
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	FORCEINLINE void RegisterMessageHandler(uint32 MsgType, google::protobuf::Message* MessageTemplate, const FChanneldMessageHandlerFunc& Handler = nullptr)
	{
		MessageHandlerEntry& Entry = MessageHandlers.FindOrAdd(MsgType);
		Entry.Msg = MessageTemplate;
		if (Handler)
		{
			Entry.Handlers.Add(Handler);
		}
	}

	template <typename UserClass>
	FORCEINLINE void RegisterMessageHandler(uint32 MsgType, google::protobuf::Message* MessageTemplate, UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void(UChanneldConnection*, Channeld::ChannelId, const google::protobuf::Message*)>::Type InFunc)
	{
		MessageHandlerEntry& Entry = MessageHandlers.FindOrAdd(MsgType);
		Entry.Msg = MessageTemplate;
		Entry.Delegate.AddUObject(InUserObject, InFunc);
	}

	FORCEINLINE void AddMessageHandler(uint32 MsgType, const FChanneldMessageHandlerFunc& Handler)
	{
		MessageHandlerEntry& Entry = MessageHandlers.FindOrAdd(MsgType);
		if (Entry.Msg == nullptr)
		{
			UE_LOG(LogChanneld, Error, TEXT("No message template registered for msgType: %d"), MsgType);
			return;
		}
		Entry.Handlers.Add(Handler);
	}

	template <typename UserClass>
	FORCEINLINE void AddMessageHandler(uint32 MsgType, UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void(UChanneldConnection*, uint32, const google::protobuf::Message*)>::Type InFunc)
	{
		MessageHandlerEntry& Entry = MessageHandlers.FindOrAdd(MsgType);
		if (Entry.Msg == nullptr)
		{
			UE_LOG(LogChanneld, Error, TEXT("No message template registered for msgType: %d"), MsgType);
			return;
		}
		Entry.Delegate.AddUObject(InUserObject, InFunc);
	}

	void RemoveMessageHandler(uint32 MsgType, const void* InUserObject)
	{
		auto Entry = MessageHandlers.Find(MsgType);
		if (Entry == nullptr)
		{
			UE_LOG(LogChanneld, Warning, TEXT("Failed to remove message handler as the msgType is not found: %d"), MsgType);
			return;
		}
		Entry->Delegate.RemoveAll(InUserObject);
	}

	FORCEINLINE FSocket* GetSocket() { return Socket; }
	
	FORCEINLINE Channeld::ConnectionId GetConnId()
	{
		ensureMsgf(ConnId != 0, TEXT("ConnId is 0 which means the connection is not authorized yet"));
		return ConnId;
	}

	FORCEINLINE bool IsConnected() { return !IsPendingKill() && Socket != nullptr && Socket->GetConnectionState() == SCS_Connected; }

	FORCEINLINE bool IsAuthenticated() { return ConnId > 0; }

	FORCEINLINE channeldpb::ConnectionType GetConnectionType() { return ConnectionType; }
	FORCEINLINE bool IsServer() { return ConnectionType == channeldpb::SERVER; }
	FORCEINLINE bool IsClient() { return ConnectionType == channeldpb::CLIENT; }

	FORCEINLINE FInternetAddr& GetRemoteAddr() const { return *RemoteAddr; }

	//virtual void BeginDestroy() override;

	bool Connect(bool bInitAsClient, const FString& Host, int32 Port, FString& Error);
	void Disconnect(bool bFlushAll = true);
	// Send a message to channeld. Thread-safe.
	void Send(Channeld::ChannelId ChId, uint32 MsgType, google::protobuf::Message& Msg, channeldpb::BroadcastType Broadcast = channeldpb::NO_BROADCAST, const FChanneldMessageHandlerFunc& HandlerFunc = nullptr);
	// Send with underlying bytes.
	void SendRaw(Channeld::ChannelId ChId, uint32 MsgType, const std::string& MsgBody, channeldpb::BroadcastType Broadcast = channeldpb::NO_BROADCAST, const FChanneldMessageHandlerFunc& HandlerFunc = nullptr);
	// Send a message that wrapped by the ServerForwardMessage to a specific connection. If ClientConnId is 0, the message will be forwarded to the channel owner.
	void Forward(Channeld::ChannelId ChId, uint32 MsgType, const google::protobuf::Message& Msg, Channeld::ConnectionId ClientConnId = 0);
	// Send a message that wrapped by the ServerForwardMessage. This is mainly for using channeld for broadcasting.
	void Broadcast(Channeld::ChannelId ChId, uint32 MsgType, const google::protobuf::Message& Msg, int BroadcastType);
	/**
	 * @brief Server sends a DisconnectMessage for safe disconnection. The message skips queueing and will be sent immediately.
	 * @param InConnId The Id of the Connection to be disconnected
	 */
	void SendDisconnectMessage(Channeld::ConnectionId InConnId);
	
	void Auth(const FString& PIT, const FString& LT, const TFunction<void(const channeldpb::AuthResultMessage*)>& Callback = nullptr);
	void CreateChannel(channeldpb::ChannelType ChannelType, const FString& Metadata, const channeldpb::ChannelSubscriptionOptions* SubOptions = nullptr, const google::protobuf::Message* Data = nullptr, const channeldpb::ChannelDataMergeOptions* MergeOptions = nullptr, const TFunction<void(const channeldpb::CreateChannelResultMessage*)>& Callback = nullptr);
	void CreateSpatialChannel(const FString& Metadata, const channeldpb::ChannelSubscriptionOptions* SubOptions = nullptr, const google::protobuf::Message* Data = nullptr, const channeldpb::ChannelDataMergeOptions* MergeOptions = nullptr, const TFunction<void(const channeldpb::CreateSpatialChannelsResultMessage*)>& Callback = nullptr);
	/**
	 * Remove Channel by channel Id
	 *
	 * @param ChannelToRemove   The channel to remove, make sure the invoker is the owner of the channel or owner of global
	 */
	void RemoveChannel(uint32 ChannelToRemove, const TFunction<void(const channeldpb::RemoveChannelMessage*)>& Callback = nullptr);
	void ListChannel(channeldpb::ChannelType TypeFilter = channeldpb::UNKNOWN, const TArray<FString>* MetadataFilters = nullptr, const TFunction<void(const channeldpb::ListChannelResultMessage*)>& Callback = nullptr);
	void SubToChannel(Channeld::ChannelId ChId, const channeldpb::ChannelSubscriptionOptions* SubOptions = nullptr, const TFunction<void(const channeldpb::SubscribedToChannelResultMessage*)>& Callback = nullptr);
	void SubConnectionToChannel(Channeld::ConnectionId ConnId, Channeld::ChannelId ChId, const channeldpb::ChannelSubscriptionOptions* SubOptions = nullptr, const TFunction<void(const channeldpb::SubscribedToChannelResultMessage*)>& Callback = nullptr);
	void UnsubFromChannel(Channeld::ChannelId ChId, const TFunction<void(const channeldpb::UnsubscribedFromChannelResultMessage*)>& Callback = nullptr);
	void UnsubConnectionFromChannel(Channeld::ConnectionId ConnId, Channeld::ChannelId ChId, const TFunction<void(const channeldpb::UnsubscribedFromChannelResultMessage*)>& Callback = nullptr);
	void QuerySpatialChannel(const TArray<FVector>& Positions, const TFunction<void(const channeldpb::QuerySpatialChannelResultMessage*)>& Callback = nullptr);
	
	void TickIncoming();
	void TickOutgoing();

	UPROPERTY(Config)
		int32 ReceiveBufferSize = Channeld::MaxPacketSize;
	
	UPROPERTY(Config)
		bool bShowUserSpaceMessageLog = false;

	FChanneldAuthenticatedDelegate OnAuthenticated;

	//FUserSpaceMessageHandlerFunc UserSpaceMessageHandlerFunc = nullptr;
	FUserSpaceMessageDelegate OnUserSpaceMessageReceived;

	TMap<Channeld::ChannelId, FSubscribedChannelInfo> SubscribedChannels;
	TMap<Channeld::ChannelId, FOwnedChannelInfo> OwnedChannels;
	TMap<Channeld::ChannelId, FListedChannelInfo> ListedChannels;

private:
	const uint32 HeaderSize = 5;

	channeldpb::ConnectionType ConnectionType = channeldpb::NO_CONNECTION;
	channeldpb::CompressionType CompressionType = channeldpb::NO_COMPRESSION;
	Channeld::ConnectionId ConnId = 0;
	TSharedPtr<FInternetAddr> RemoteAddr;
	FSocket* Socket;

	FThreadSafeBool bReceiveThreadRunning = false;
	FRunnableThread* ReceiveThread = nullptr;
	uint8* ReceiveBuffer;
	uint32 ReceiveBufferOffset;

	struct MessageHandlerEntry
	{
		google::protobuf::Message* Msg;
		TArray<FChanneldMessageHandlerFunc> Handlers;
		FChanneldMessageDelegate Delegate;
	};

	struct MessageQueueEntry
	{
		uint32 MsgType;
		google::protobuf::Message* Msg;
		Channeld::ChannelId ChId;
		uint32 StubId;
		TArray<FChanneldMessageHandlerFunc> Handlers;
		FChanneldMessageDelegate Delegate;
	};

	MessageHandlerEntry UserSpaceMessageHandlerEntry;
	TMap<uint32, MessageHandlerEntry> MessageHandlers;
	TQueue<MessageQueueEntry> IncomingQueue;
	TQueue<TSharedPtr<channeldpb::MessagePack>> OutgoingQueue;
	TMap<uint32, FChanneldMessageHandlerFunc> RpcCallbacks;

	void SendDirect(channeldpb::Packet Packet);
	void Receive();
	void OnDisconnected();

	bool StartReceiveThread();
	void StopReceiveThread();
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

	uint32 AddRpcCallback(const FChanneldMessageHandlerFunc& HandlerFunc);

	void HandleServerForwardMessage(UChanneldConnection* Conn, Channeld::ChannelId ChId, const google::protobuf::Message* Msg, uint32 MsgType);
	void HandleAuth(UChanneldConnection* Conn, Channeld::ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleCreateChannel(UChanneldConnection* Conn, Channeld::ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleRemoveChannel(UChanneldConnection* Conn, Channeld::ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleListChannel(UChanneldConnection* Conn, Channeld::ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleSubToChannel(UChanneldConnection* Conn, Channeld::ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleUnsubFromChannel(UChanneldConnection* Conn, Channeld::ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleChannelDataUpdate(UChanneldConnection* Conn, Channeld::ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleCreateSpatialChannel(UChanneldConnection* Conn, Channeld::ChannelId ChId, const google::protobuf::Message* Msg);
};