#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "google/protobuf/message.h"
#include "ChanneldTypes.h"
#include "Channeld.pb.h"
#include "ChanneldConnection.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogChanneld, Log, All);

DECLARE_MULTICAST_DELEGATE_ThreeParams(FChanneldMessageDelegate, UChanneldConnection*, ChannelId, const google::protobuf::Message*)

typedef TFunction<void(UChanneldConnection*, ChannelId, const google::protobuf::Message*)> MessageHandlerFunc;

/*
template<typename T>
struct MessageHandlerFunc : TMemFunPtrType<false, T, void(UChanneldConnection*, uint32, const google::protobuf::Message*)>::Type
{

};
*/

UCLASS(transient, config=Engine)
class CHANNELDUE_API UChanneldConnection : public UObject
{
	GENERATED_BODY()

public:

	// Constructors.
	UChanneldConnection(const FObjectInitializer& ObjectInitializer);

	void InitSocket(ISocketSubsystem* InSocketSubsystem);

	template <typename UserClass>
	FORCEINLINE void RegisterMessageHandler(uint32 MsgType, google::protobuf::Message* MessageTemplate, UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void (UChanneldConnection*, ChannelId, const google::protobuf::Message*)>::Type InFunc)
	{
		MessageHandlerEntry& Entry = MessageHandlers.FindOrAdd(MsgType);
		Entry.msg = MessageTemplate;
		Entry.handler.AddUObject(InUserObject, InFunc);
	}
    
	template <typename UserClass>
	FORCEINLINE void AddMessageHandler(uint32 MsgType, UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void (UChanneldConnection*, uint32, const google::protobuf::Message*)>::Type InFunc)
	{
		MessageHandlerEntry& Entry = MessageHandlers.FindOrAdd(MsgType);
		if (Entry.msg == nullptr)
		{
			UE_LOG(LogChanneld, Error, TEXT("No message template registered for msgType: %d"), MsgType);
			return;
		}
		Entry.handler.AddUObject(InUserObject, InFunc);
	}

	//FORCEINLINE FSocket* GetSocket() { return Socket; }
	FORCEINLINE ConnectionId GetConnId() { return ConnId; }
	FORCEINLINE bool IsConnected() { return Socket->GetConnectionState() == SCS_Connected; }

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
	int32 ReceiveBufferSize;

private:
    channeldpb::ConnectionType ConnectionType;
	channeldpb::CompressionType CompressionType;
	ConnectionId ConnId;
	ISocketSubsystem* SocketSubsystem;
	TSharedPtr<FInternetAddr> RemoteAddr;
	FUniqueSocket Socket;
	uint8* ReceiveBuffer;
	int ReceiveBufferOffset;

	struct MessageHandlerEntry
	{
		google::protobuf::Message* msg;
		FChanneldMessageDelegate handler;
	};

	struct MessageQueueEntry
	{
		google::protobuf::Message* msg;
		ChannelId channelId;
		uint32 stubId;
		FChanneldMessageDelegate handler;
	};

	struct RpcCallback
	{
		FChanneldMessageDelegate handler;
	};

	TMap<uint32, MessageHandlerEntry> MessageHandlers;

	TQueue<MessageQueueEntry> IncomingQueue;

	TQueue<channeldpb::MessagePack> OutgoingQueue;
	
	TMap<uint32, RpcCallback> RpcCallbacks;

	void Receive();
	uint32 AddRpcCallback(const MessageHandlerFunc& HandlerFunc);

	void HandleAuthResult(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleCreateChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleSubToChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleChannelDataUpdate(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);

};