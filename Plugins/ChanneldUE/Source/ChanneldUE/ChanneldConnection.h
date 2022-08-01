#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"
#include "google/protobuf/message.h"
#include "ChanneldTypes.h"
#include "Channeld.pb.h"
#include "ChanneldConnection.generated.h"

class UChanneldConnection;

DECLARE_LOG_CATEGORY_EXTERN(LogChanneld, Log, All);

DECLARE_MULTICAST_DELEGATE_ThreeParams(FChanneldMessageDelegate, UChanneldConnection*, ChannelId, const google::protobuf::Message*)
//DECLARE_MULTICAST_DELEGATE_ThreeParams(FUserSpaceMessageDelegate, ChannelId, ConnectionId, const std::string&)
DECLARE_MULTICAST_DELEGATE_OneParam(FChanneldAuthenticatedDelegate, UChanneldConnection*);

typedef TFunction<void(UChanneldConnection*, ChannelId, const google::protobuf::Message*)> FChanneldMessageHandlerFunc;
typedef TFunction<void(ChannelId, ConnectionId, const std::string&)> FUserSpaceMessageHandlerFunc;

UCLASS(transient, config = ChanneldUE)
class CHANNELDUE_API UChanneldConnection : public UObject, public FRunnable
{
	GENERATED_BODY()

public:

	// Constructors.
	UChanneldConnection(const FObjectInitializer& ObjectInitializer);

	FORCEINLINE void RegisterMessageHandler(uint32 MsgType, google::protobuf::Message* MessageTemplate, const FChanneldMessageHandlerFunc& Handler)
	{
		MessageHandlerEntry& Entry = MessageHandlers.FindOrAdd(MsgType);
		Entry.Msg = MessageTemplate;
		Entry.Handlers.Add(Handler);
	}

	template <typename UserClass>
	FORCEINLINE void RegisterMessageHandler(uint32 MsgType, google::protobuf::Message* MessageTemplate, UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void(UChanneldConnection*, ChannelId, const google::protobuf::Message*)>::Type InFunc)
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

	FORCEINLINE void RemoveMessageHandler(uint32 MsgType, const void* InUserObject)
	{
		auto Entry = MessageHandlers.Find(MsgType);
		if (Entry == nullptr)
		{
			UE_LOG(LogChanneld, Warning, TEXT("Failed to remove message handler as the msgType is not found: %d"), MsgType);
			return;
		}
		Entry->Delegate.RemoveAll(InUserObject);
	}

	//FORCEINLINE FSocket* GetSocket() { return Socket; }
	FORCEINLINE ConnectionId GetConnId()
	{
		ensureMsgf(ConnId != 0, TEXT("ConnId is 0 which means the connection is not authorized yet"));
		return ConnId;
	}

	FORCEINLINE bool IsConnected() { return !IsPendingKill() && Socket != nullptr && Socket->GetConnectionState() == SCS_Connected && bReceiveThreadRunning; }

	FORCEINLINE bool IsAuthenticated() { return ConnId > 0; }

	FORCEINLINE channeldpb::ConnectionType GetConnectionType() { return ConnectionType; }
	FORCEINLINE bool IsServer() { return ConnectionType == channeldpb::SERVER; }
	FORCEINLINE bool IsClient() { return ConnectionType == channeldpb::CLIENT; }

	FORCEINLINE FInternetAddr& GetRemoteAddr() const { return *RemoteAddr; }

	virtual void BeginDestroy() override;

	bool Connect(bool bInitAsClient, const FString& Host, int Port, FString& Error);
	void Disconnect(bool bFlushAll = true);
	// Thread-safe
	void Send(ChannelId ChId, uint32 MsgType, google::protobuf::Message& Msg, channeldpb::BroadcastType Broadcast = channeldpb::NO_BROADCAST, const FChanneldMessageHandlerFunc& HandlerFunc = nullptr);
	void SendRaw(ChannelId ChId, uint32 MsgType, const uint8* MsgBody, const int32 BodySize, channeldpb::BroadcastType Broadcast = channeldpb::NO_BROADCAST, const FChanneldMessageHandlerFunc& HandlerFunc = nullptr);

	void Auth(const FString& PIT, const FString& LT, const TFunction<void(const channeldpb::AuthResultMessage*)>& Callback = nullptr);
	void CreateChannel(channeldpb::ChannelType ChannelType, const FString& Metadata, channeldpb::ChannelSubscriptionOptions* SubOptions = nullptr, const google::protobuf::Message* Data = nullptr, channeldpb::ChannelDataMergeOptions* MergeOptions = nullptr, const TFunction<void(const channeldpb::CreateChannelResultMessage*)>& Callback = nullptr);

	/**
	 * Remove Channel by channel Id
	 *
	 * @param ChannelToRemove   The channel to remove, make sure the invoker is the owner of the channel or owner of global
	 */
	void RemoveChannel(uint32 ChannelToRemove, const TFunction<void(const channeldpb::RemoveChannelMessage*)>& Callback = nullptr);

	void SubToChannel(ChannelId ChId, channeldpb::ChannelSubscriptionOptions* SubOptions = nullptr, const TFunction<void(const channeldpb::SubscribedToChannelResultMessage*)>& Callback = nullptr);
	void SubConnectionToChannel(ConnectionId ConnId, ChannelId ChId, channeldpb::ChannelSubscriptionOptions* SubOptions = nullptr, const TFunction<void(const channeldpb::SubscribedToChannelResultMessage*)>& Callback = nullptr);

	void TickIncoming();
	void TickOutgoing();

	UPROPERTY(Config)
		int32 ReceiveBufferSize = MaxPacketSize;

	UPROPERTY(Config)
		bool bShowUserSpaceMessageLog = false;

	FChanneldAuthenticatedDelegate OnAuthenticated;

	FUserSpaceMessageHandlerFunc UserSpaceMessageHandlerFunc = nullptr;

	TMap<ChannelId, FSubscribedChannelInfo> SubscribedChannels;
	TMap<ChannelId, FOwnedChannelInfo> OwnedChannels;
	TMap<ChannelId, FListedChannelInfo> ListedChannels;

private:
	channeldpb::ConnectionType ConnectionType = channeldpb::NO_CONNECTION;
	channeldpb::CompressionType CompressionType = channeldpb::NO_COMPRESSION;
	ConnectionId ConnId = 0;
	TSharedPtr<FInternetAddr> RemoteAddr;
	FSocket* Socket;

	FThreadSafeBool bReceiveThreadRunning = false;
	FRunnableThread* ReceiveThread = nullptr;
	uint8* ReceiveBuffer;
	int ReceiveBufferOffset;

	struct MessageHandlerEntry
	{
		google::protobuf::Message* Msg;
		TArray<FChanneldMessageHandlerFunc> Handlers;
		FChanneldMessageDelegate Delegate;
	};

	struct MessageQueueEntry
	{
		google::protobuf::Message* Msg;
		ChannelId ChId;
		uint32 StubId;
		TArray<FChanneldMessageHandlerFunc> Handlers;
		FChanneldMessageDelegate Delegate;
	};

	MessageHandlerEntry UserSpaceMessageHandlerEntry;
	TMap<uint32, MessageHandlerEntry> MessageHandlers;
	TQueue<MessageQueueEntry> IncomingQueue;
	TQueue<channeldpb::MessagePack> OutgoingQueue;
	TMap<uint32, FChanneldMessageHandlerFunc> RpcCallbacks;

	void Receive();
	void OnDisconnected();

	bool StartReceiveThread();
	void StopReceiveThread();
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

	uint32 AddRpcCallback(const FChanneldMessageHandlerFunc& HandlerFunc);

	void HandleServerForwardMessage(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleAuth(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleCreateChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleRemoveChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleListChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleSubToChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleUnsubFromChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleChannelDataUpdate(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);

};