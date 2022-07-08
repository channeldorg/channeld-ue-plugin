#include "ChanneldConnection.h"

#include "ChanneldNetDriver.h"
#include "SocketSubsystem.h"

DEFINE_LOG_CATEGORY(LogChanneld);

UChanneldConnection::UChanneldConnection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (ReceiveBufferSize > MaxPacketSize)
		ReceiveBufferSize = MaxPacketSize;
	ReceiveBuffer = new uint8[ReceiveBufferSize];

	// StubId=0 is reserved.
	RpcCallbacks.Add(0, nullptr);

	// The connection's internal handlers should always be called first, so we should not use the delegate as the order of its broadcast is not guaranteed.
	RegisterMessageHandler((uint32)channeldpb::AUTH, new channeldpb::AuthResultMessage(), [&](UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
		{
			HandleAuthResult(Conn, ChId, Msg);
		});
	RegisterMessageHandler((uint32)channeldpb::CREATE_CHANNEL, new channeldpb::CreateChannelMessage(), [&](UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
		{
			HandleCreateChannel(Conn, ChId, Msg);
		});
	RegisterMessageHandler((uint32)channeldpb::SUB_TO_CHANNEL, new channeldpb::SubscribedToChannelResultMessage(), [&](UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
		{
			HandleSubToChannel(Conn, ChId, Msg);
		});
	RegisterMessageHandler((uint32)channeldpb::CHANNEL_DATA_UPDATE, new channeldpb::ChannelDataUpdateMessage(), [&](UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
		{
			HandleChannelDataUpdate(Conn, ChId, Msg);
		});
}

void UChanneldConnection::BeginDestroy()
{
	Super::BeginDestroy();
	Disconnect(false);
}

bool UChanneldConnection::Connect(bool bInitAsClient, const FString& Host, int Port, FString& Error)
{
	auto SocketSubsystem = ISocketSubsystem::Get();
	if (SocketSubsystem == NULL)
	{
		Error = TEXT("Unable to find socket subsystem");
		return false;
	}

	RemoteAddr = SocketSubsystem->CreateInternetAddr();
	bool IsIpValid;
	if (bInitAsClient)
	{
        ConnectionType = channeldpb::CLIENT;
	}
	else
	{
        ConnectionType = channeldpb::SERVER;
	}
    RemoteAddr->SetIp(*Host, IsIpValid);
    if (!IsIpValid)
    {
        Error = FString::Printf(TEXT("Invalid IP for client: %s"), *Host);
        return false;
    }
    RemoteAddr->SetPort(Port);

	// Create TCP socket to channeld
	Socket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("Connection to channeld"), RemoteAddr->GetProtocolType());
	UE_LOG(LogChanneld, Log, TEXT("Connecting to channeld with addr: %s"), *RemoteAddr->ToString(true));
	
    return Socket->Connect(*RemoteAddr);
}

void UChanneldConnection::Disconnect(bool bFlushAll/* = true*/)
{
	if (!IsConnected())
		return;

	if (bFlushAll)
	{
		TickOutgoing();
		// TODO: Flush?
	}

	Socket->Close();
}


void UChanneldConnection::Receive()
{
	uint32 PendingDataSize;
	if (!Socket->HasPendingData(PendingDataSize))
		return;

	int32 BytesRead;
	if (Socket->Recv(ReceiveBuffer + ReceiveBufferOffset, ReceiveBufferSize, BytesRead, ESocketReceiveFlags::None))
	{
		ReceiveBufferOffset += BytesRead;
		if (BytesRead < 5)
		{
			// Unfinished packet
			return;
		}

		if (ReceiveBuffer[0] != 67)
		{
			ReceiveBufferOffset = 0;
			UE_LOG(LogChanneld, Error, TEXT("Invalid tag: %d, the packet will be dropped"), ReceiveBuffer[0]);
			return;
		}

		int32 PacketSize = ReceiveBuffer[3];
		if (ReceiveBuffer[1] != 72)
			PacketSize = PacketSize | (ReceiveBuffer[1] << 16) | (ReceiveBuffer[2] << 8);
		else if (ReceiveBuffer[2] != 78)
			PacketSize = PacketSize | (ReceiveBuffer[2] << 8);

		if (BytesRead < 5 + PacketSize)
		{
			// Unfinished packet
			return;
		}

		// TODO: support Snappy compression

		channeldpb::Packet Packet;
		if (!Packet.ParseFromArray(ReceiveBuffer + 5, PacketSize))
		{
			ReceiveBufferOffset = 0;
			UE_LOG(LogChanneld, Error, TEXT("Failed to parse packet"));
			return;
		}

		for (auto const MessagePack : Packet.messages())
		{
			uint32 MsgType = MessagePack.msgtype();

			if (!MessageHandlers.Contains(MsgType))
			{
				UE_LOG(LogChanneld, Warning, TEXT("No message handler registered for type: %d"), MessagePack.msgtype());
				continue;
			}
			auto Entry = MessageHandlers[MsgType];
			if (Entry.Msg == nullptr)
			{
				UE_LOG(LogChanneld, Error, TEXT("No message template registered for type: %d"), MessagePack.msgtype());
				continue;
			}

			// Always make a clone!
			google::protobuf::Message* Msg = Entry.Msg->New();
			Msg->CopyFrom(*Entry.Msg);
			if (!Msg->ParseFromString(MessagePack.msgbody()))
			{
				UE_LOG(LogChanneld, Error, TEXT("Failed to parse message %s"), Msg->GetTypeName().c_str());
				continue;
			}

			MessageQueueEntry QueueEntry = { Msg, MessagePack.channelid(), MessagePack.stubid(), Entry.Handlers, Entry.Delegate };
			IncomingQueue.Enqueue(QueueEntry);
		}

	}
	else
	{
		// Handle disconnection or exception
		UE_LOG(LogChanneld, Warning, TEXT("Failed to receive data from channeld"));
	}

	// Reset read position
	ReceiveBufferOffset = 0;
}

uint32 UChanneldConnection::AddRpcCallback(const MessageHandlerFunc& HandlerFunc)
{
	uint32 StubId = 0;
	while (RpcCallbacks.Contains(StubId))
	{
		StubId++;
	}

	RpcCallbacks.Add(StubId, HandlerFunc);
	return StubId;
}

void UChanneldConnection::TickIncoming()
{
	Receive();

	MessageQueueEntry Entry;
	while (IncomingQueue.Dequeue(Entry))
	{
		// Handler functions are called before the delegate.Broadcast()
		for (const auto Func : Entry.Handlers)
		{
			Func(this, Entry.ChId, Entry.Msg);
		}
		Entry.Delegate.Broadcast(this, Entry.ChId, Entry.Msg);

		if (Entry.StubId > 0)
		{
			auto Func = RpcCallbacks.Find(Entry.StubId);
			if (Func != nullptr)
			{
				UE_LOG(LogChanneld, Verbose, TEXT("Handling RPC callback of %s, stubId: %d"), Entry.Msg->GetTypeName().c_str(), Entry.StubId);
				(*Func)(this, Entry.ChId, Entry.Msg);
				RpcCallbacks.Remove(Entry.StubId);
			}
		}
	}
}

void UChanneldConnection::TickOutgoing()
{
	if (!IsConnected())
		return;

	if (OutgoingQueue.IsEmpty())
		return;

	const uint32 HeaderSize = 5;
	channeldpb::Packet Packet;
	uint32 Size = HeaderSize;
	channeldpb::MessagePack MessagePack;
	while (OutgoingQueue.Dequeue(MessagePack))
	{
		Size += MessagePack.ByteSizeLong();
		if (Size >= MaxPacketSize)
			break;
		Packet.add_messages()->CopyFrom(MessagePack);
	}

	int PacketSize = Packet.ByteSizeLong();
	Size = PacketSize + HeaderSize;
	// TODO: Use a send buffer for all transmissions instead of temp buffer for each transmission
	uint8* PacketData = new uint8[Size];
	if (!Packet.SerializeToArray(PacketData + 5, Size))
	{
		delete[] PacketData;
		UE_LOG(LogChanneld, Error, TEXT("Failed to serialize Packet, size: %d"), Size);
		return;
	}

	// Set the header
	PacketData[0] = 67;
	PacketData[1] = PacketSize > 0xffff ? (0xff & (PacketSize >> 16)) : 72;
	PacketData[2] = PacketSize > 0xff ? (0xff & (PacketSize >> 8)) : 78;
	PacketData[3] = (uint8)(PacketSize & 0xff);
	// TODO: support Snappy compression
	PacketData[4] = 0;

	int32 BytesSent;
	bool IsSent = Socket->Send(PacketData, Size, BytesSent);
	// Free send buffer
	delete[] PacketData;
	if (!IsSent)
	{
		UE_LOG(LogChanneld, Error, TEXT("Failed to send packet to channeld, size: %d"), Size);
	}
}

void UChanneldConnection::Send(ChannelId ChId, uint32 MsgType, google::protobuf::Message& Msg, channeldpb::BroadcastType Broadcast/* = channeldpb::NO_BROADCAST*/, const MessageHandlerFunc& HandlerFunc/* = nullptr*/)
{
	// TODO: use a serialization buffer as the member variable
	uint8* MessageData = new uint8[Msg.ByteSizeLong()];
	bool Serialized = Msg.SerializeToArray(MessageData, Msg.GetCachedSize());
	if (!Serialized)
	{
		delete[] MessageData;
		UE_LOG(LogChanneld, Error, TEXT("Failed to serialize message, type: %d"), MsgType);
		return;
	}

	uint32 StubId = HandlerFunc != nullptr ? AddRpcCallback(HandlerFunc) : 0;

	channeldpb::MessagePack MsgPack;
	MsgPack.set_channelid(ChId);
	MsgPack.set_broadcast(Broadcast);
	MsgPack.set_stubid(StubId);
	MsgPack.set_msgtype(MsgType);
	MsgPack.set_msgbody(MessageData, Msg.GetCachedSize());

	OutgoingQueue.Enqueue(MsgPack);
}

template <typename MsgClass>
MessageHandlerFunc WrapMessageHandler(const TFunction<void(const MsgClass*)>& Callback)
{
	if (Callback == nullptr)
		return nullptr;
	return [Callback](UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
	{
		Callback(static_cast<const MsgClass*>(Msg));
	};
}

void UChanneldConnection::Auth(const FString& PIT, const FString& LT, const TFunction<void(const channeldpb::AuthResultMessage*)>& Callback /*= nullptr*/)
{
	channeldpb::AuthMessage Msg;
	Msg.set_playeridentifiertoken(std::string(TCHAR_TO_UTF8(*PIT)));
	Msg.set_logintoken(std::string(TCHAR_TO_UTF8(*LT)));
	
	Send(GlobalChannelId, channeldpb::AUTH, Msg, channeldpb::NO_BROADCAST, WrapMessageHandler(Callback));
}

void UChanneldConnection::CreateChannel(channeldpb::ChannelType ChannelType, const FString& Metadata, channeldpb::ChannelSubscriptionOptions* SubOptions /*= nullptr*/, const google::protobuf::Message* Data /*= nullptr*/, channeldpb::ChannelDataMergeOptions* MergeOptions /*= nullptr*/, const TFunction<void(const channeldpb::CreateChannelResultMessage*)>& Callback /*= nullptr*/)
{
	channeldpb::CreateChannelMessage Msg;
	Msg.set_channeltype(ChannelType);
	Msg.set_metadata(TCHAR_TO_UTF8(*Metadata));
	if (SubOptions != nullptr)
		Msg.set_allocated_suboptions(SubOptions);
	if (Data != nullptr)
		Msg.mutable_data()->PackFrom(*Data);
	if (MergeOptions != nullptr)
		Msg.set_allocated_mergeoptions(MergeOptions);
	
	Send(GlobalChannelId, channeldpb::CREATE_CHANNEL, Msg, channeldpb::NO_BROADCAST, WrapMessageHandler(Callback));
}

void UChanneldConnection::SubToChannel(ChannelId ChId, channeldpb::ChannelSubscriptionOptions* SubOptions /*= nullptr*/, const TFunction<void(const channeldpb::SubscribedToChannelResultMessage*)>& Callback /*= nullptr*/)
{
	SubConnectionToChannel(GetConnId(), ChId, SubOptions, Callback);
}

void UChanneldConnection::SubConnectionToChannel(ConnectionId TargetConnId, ChannelId ChId, channeldpb::ChannelSubscriptionOptions* SubOptions /*= nullptr*/, const TFunction<void(const channeldpb::SubscribedToChannelResultMessage*)>& Callback /*= nullptr*/)
{
	channeldpb::SubscribedToChannelMessage Msg;
	Msg.set_connid(TargetConnId);
	if (SubOptions != nullptr)
		Msg.set_allocated_suboptions(SubOptions);

	Send(ChId, channeldpb::SUB_TO_CHANNEL, Msg, channeldpb::NO_BROADCAST, WrapMessageHandler(Callback));
}

void UChanneldConnection::HandleAuthResult(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto ResultMsg = static_cast<const channeldpb::AuthResultMessage*>(Msg);
	if (ResultMsg->result() == channeldpb::AuthResultMessage_AuthResult_SUCCESSFUL)
	{
		if (ConnId == 0)
		{
			ConnId = ResultMsg->connid();
			CompressionType = ResultMsg->compressiontype();
		}
	}
}

void UChanneldConnection::HandleCreateChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{

}

void UChanneldConnection::HandleSubToChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{

}

void UChanneldConnection::HandleChannelDataUpdate(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{

}

