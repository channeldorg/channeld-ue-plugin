#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"
#include "google/protobuf/message.h"
#include "ChanneldTypes.h"
#include "Channeld.pb.h"
#include "ChanneldConnection.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogChanneld, Log, All);

DECLARE_MULTICAST_DELEGATE_ThreeParams(FChanneldMessageDelegate, UChanneldConnection*, ChannelId, const google::protobuf::Message*)

typedef TFunction<void(UChanneldConnection*, ChannelId, const google::protobuf::Message*)> MessageHandlerFunc;

UCLASS(transient, config=Engine)
class CHANNELDUE_API UChanneldConnection : public UObject
{
	GENERATED_BODY()

public:

	// Constructors.
	UChanneldConnection(const FObjectInitializer& ObjectInitializer);

	FORCEINLINE void RegisterMessageHandler(uint32 MsgType, google::protobuf::Message* MessageTemplate, const MessageHandlerFunc& Handler)
	{
		MessageHandlerEntry& Entry = MessageHandlers.FindOrAdd(MsgType);
		Entry.Msg = MessageTemplate;
		Entry.Handlers.Add(Handler);
	}

	template <typename UserClass>
	FORCEINLINE void RegisterMessageHandler(uint32 MsgType, google::protobuf::Message* MessageTemplate, UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void (UChanneldConnection*, ChannelId, const google::protobuf::Message*)>::Type InFunc)
	{
		MessageHandlerEntry& Entry = MessageHandlers.FindOrAdd(MsgType);
		Entry.Msg = MessageTemplate;
		Entry.Delegate.AddUObject(InUserObject, InFunc);
	}

	FORCEINLINE void AddMessageHandler(uint32 MsgType, const MessageHandlerFunc& Handler)
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
	FORCEINLINE void AddMessageHandler(uint32 MsgType, UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void (UChanneldConnection*, uint32, const google::protobuf::Message*)>::Type InFunc)
	{
		MessageHandlerEntry& Entry = MessageHandlers.FindOrAdd(MsgType);
		if (Entry.Msg == nullptr)
		{
			UE_LOG(LogChanneld, Error, TEXT("No message template registered for msgType: %d"), MsgType);
			return;
		}
		Entry.Delegate.AddUObject(InUserObject, InFunc);
	}

	//FORCEINLINE FSocket* GetSocket() { return Socket; }
	FORCEINLINE ConnectionId GetConnId()
	{
		ensureMsgf(ConnId != 0, TEXT("ConnId is 0 which means the connection is not authorized yet"));
		return ConnId;
	}
	FORCEINLINE bool IsConnected() { return !IsPendingKill() && Socket != nullptr && Socket->GetConnectionState() == SCS_Connected; }

	virtual void BeginDestroy() override;

    bool Connect(bool bInitAsClient, const FString& Host, int Port, FString& Error);
    void Disconnect(bool bFlushAll = true);
	// Thread-safe
	void Send(ChannelId ChId, uint32 MsgType, google::protobuf::Message& Msg, channeldpb::BroadcastType Broadcast = channeldpb::NO_BROADCAST, const MessageHandlerFunc& HandlerFunc = nullptr);

	void Auth(const FString& PIT, const FString& LT, const TFunction<void(const channeldpb::AuthResultMessage*)>& Callback = nullptr);
	void CreateChannel(channeldpb::ChannelType ChannelType, const FString& Metadata, channeldpb::ChannelSubscriptionOptions* SubOptions = nullptr, const google::protobuf::Message* Data = nullptr, channeldpb::ChannelDataMergeOptions* MergeOptions = nullptr, const TFunction<void(const channeldpb::CreateChannelResultMessage*)>& Callback = nullptr);
	void SubToChannel(ChannelId ChId, channeldpb::ChannelSubscriptionOptions* SubOptions = nullptr, const TFunction<void(const channeldpb::SubscribedToChannelResultMessage*)>& Callback = nullptr);
	void SubConnectionToChannel(ConnectionId ConnId, ChannelId ChId, channeldpb::ChannelSubscriptionOptions* SubOptions = nullptr, const TFunction<void(const channeldpb::SubscribedToChannelResultMessage*)>& Callback = nullptr);

	void TickIncoming();
	void TickOutgoing();

	UPROPERTY(Config)
	int32 ReceiveBufferSize = MaxPacketSize;

private:
    channeldpb::ConnectionType ConnectionType;
	channeldpb::CompressionType CompressionType;
	ConnectionId ConnId;
	TSharedPtr<FInternetAddr> RemoteAddr;
	FSocket* Socket;
	uint8* ReceiveBuffer;
	int ReceiveBufferOffset;

	struct MessageHandlerEntry
	{
		google::protobuf::Message* Msg;
		TArray<MessageHandlerFunc> Handlers;
		FChanneldMessageDelegate Delegate;
	};

	struct MessageQueueEntry
	{
		google::protobuf::Message* Msg;
		ChannelId ChId;
		uint32 StubId;
		TArray<MessageHandlerFunc> Handlers;
		FChanneldMessageDelegate Delegate;
	};

	TMap<uint32, MessageHandlerEntry> MessageHandlers;

	TQueue<MessageQueueEntry> IncomingQueue;

	TQueue<channeldpb::MessagePack> OutgoingQueue;
	
	TMap<uint32, MessageHandlerFunc> RpcCallbacks;

	void Receive();
	uint32 AddRpcCallback(const MessageHandlerFunc& HandlerFunc);

	void HandleAuthResult(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleCreateChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleSubToChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleChannelDataUpdate(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);

};