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

UCLASS(transient, config=Engine)
class CHANNELDUE_API UChanneldConnection : public UObject
{
	GENERATED_BODY()

public:

	// Constructors.
	UChanneldConnection(const FObjectInitializer& ObjectInitializer);

	template <typename UserClass, typename... VarTypes>
	FORCEINLINE void RegisterMessageHandler(uint32 MsgType, google::protobuf::Message* MessageTemplate, UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void (UChanneldConnection*, uint32, const google::protobuf::Message*, VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		MessageHandlerEntry& Entry = MessageHandlers.FindOrAdd(MsgType);
		Entry.msg = MessageTemplate;
		Entry.handler.AddUObject(InUserObject, InFunc, Vars...);
	}
    
	template <typename UserClass, typename... VarTypes>
	FORCEINLINE void AddMessageHandler(uint32 MsgType, UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void (UChanneldConnection*, uint32, const google::protobuf::Message*, VarTypes...)>::Type InFunc, VarTypes... Vars)//const FChanneldMessageDelegate::FDelegate& Handler)
	{
		MessageHandlerEntry& Entry = MessageHandlers.FindOrAdd(MsgType);
		if (Entry.msg == nullptr)
		{
			UE_LOG(LogChanneld, Error, TEXT("No message template registered for msgType: %d"), MsgType);
			return;
		}
		Entry.handler.AddUObject(InUserObject, InFunc, Vars...);
	}

	//FORCEINLINE FSocket* GetSocket() { return Socket; }
	FORCEINLINE ConnectionId GetConnId() { return ConnId; }
	FORCEINLINE bool IsConnected() { return Socket->GetConnectionState() == SCS_Connected; }

    bool Connect(bool bInitAsClient, const FString& Host, int Port, FString& Error);
    void Disconnect(bool bFlushAll = true);
	void Send(ChannelId ChId, uint32 MsgType, google::protobuf::Message& Msg, channeldpb::BroadcastType Broadcast = channeldpb::NO_BROADCAST);

	void TickIncoming();
	void TickOutgoing();

	UPROPERTY(Config)
	int32 ReceiveBufferSize;

private:
    channeldpb::ConnectionType ConnectionType;
	channeldpb::CompressionType CompressionType;
	ConnectionId ConnId;
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

	void HandleAuthResult(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleCreateChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleSubToChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleChannelDataUpdate(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);

};