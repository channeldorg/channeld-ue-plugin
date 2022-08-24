#include "ChanneldConnection.h"

#include "ChanneldNetDriver.h"
#include "SocketSubsystem.h"

DEFINE_LOG_CATEGORY(LogChanneld);

void UChanneldConnection::Initialize(FSubsystemCollectionBase& Collection)
{

//}
//
//UChanneldConnection::UChanneldConnection(const FObjectInitializer& ObjectInitializer)
//	: Super(ObjectInitializer)
//{
	if (ReceiveBufferSize > MaxPacketSize)
		ReceiveBufferSize = MaxPacketSize;
	ReceiveBuffer = new uint8[ReceiveBufferSize];

	// StubId=0 is reserved.
	RpcCallbacks.Add(0, nullptr);

	UserSpaceMessageHandlerEntry = MessageHandlerEntry();
	UserSpaceMessageHandlerEntry.Msg = new channeldpb::ServerForwardMessage;
	UserSpaceMessageHandlerEntry.Handlers.Add([&](UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
		{
			HandleServerForwardMessage(Conn, ChId, Msg);
		});

	// The connection's internal handlers should always be called first, so we should not use the delegate as the order of its broadcast is not guaranteed.
	RegisterMessageHandler((uint32)channeldpb::AUTH, new channeldpb::AuthResultMessage(), [&](UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
		{
			HandleAuth(Conn, ChId, Msg);
		});
	RegisterMessageHandler((uint32)channeldpb::CREATE_CHANNEL, new channeldpb::CreateChannelResultMessage(), [&](UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
		{
			HandleCreateChannel(Conn, ChId, Msg);
		});
	RegisterMessageHandler((uint32)channeldpb::REMOVE_CHANNEL, new channeldpb::RemoveChannelMessage(), [&](UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
		{
			HandleRemoveChannel(Conn, ChId, Msg);
		});
	RegisterMessageHandler((uint32)channeldpb::LIST_CHANNEL, new channeldpb::ListChannelResultMessage(), [&](UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
		{
			HandleListChannel(Conn, ChId, Msg);
		});
	RegisterMessageHandler((uint32)channeldpb::SUB_TO_CHANNEL, new channeldpb::SubscribedToChannelResultMessage(), [&](UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
		{
			HandleSubToChannel(Conn, ChId, Msg);
		});
	RegisterMessageHandler((uint32)channeldpb::UNSUB_FROM_CHANNEL, new channeldpb::UnsubscribedFromChannelResultMessage(), [&](UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
		{
			HandleUnsubFromChannel(Conn, ChId, Msg);
		});
	RegisterMessageHandler((uint32)channeldpb::CHANNEL_DATA_UPDATE, new channeldpb::ChannelDataUpdateMessage(), [&](UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
		{
			HandleChannelDataUpdate(Conn, ChId, Msg);
		});
}

void UChanneldConnection::Deinitialize()
{
	UserSpaceMessageHandlerEntry.Handlers.Reset();
	MessageHandlers.Reset();

//}
//
//void UChanneldConnection::BeginDestroy()
//{
//	Super::BeginDestroy();
	Disconnect(false);
}

bool UChanneldConnection::Connect(bool bInitAsClient, const FString& Host, int Port, FString& Error)
{
	if (IsConnected())
		return false;

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
	bool bSocketConnected = Socket->Connect(*RemoteAddr);
	if (!ensure(bSocketConnected))
	{
		Error = FString::Printf(TEXT("SocketConnected failed"));
		return false;
	}

	if (!ensure(StartReceiveThread()))
	{
		Error = FString::Printf(TEXT("Start receive thread failed"));
		return false;
	}
	return true;
}

void UChanneldConnection::OnDisconnected()
{
	ConnId = 0;
	ConnectionType = channeldpb::NO_CONNECTION;
	RemoteAddr = nullptr;

	IncomingQueue.Empty();
	OutgoingQueue.Empty();
	RpcCallbacks.Empty();

	OnAuthenticated.Clear();
	OnUserSpaceMessageReceived.Clear();

	SubscribedChannels.Empty();
	OwnedChannels.Empty();
	ListedChannels.Empty();
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

	OnDisconnected();

	Socket->Close();
	StopReceiveThread();

	auto SocketSubsystem = ISocketSubsystem::Get();
	if (SocketSubsystem)
	{
		SocketSubsystem->DestroySocket(Socket);
	}
	Socket = NULL;
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

			MessageHandlerEntry Entry;
			if (!MessageHandlers.Contains(MsgType))
			{
				if (MsgType >= channeldpb::USER_SPACE_START)
				{
					Entry = UserSpaceMessageHandlerEntry;
				}
				else
				{
					UE_LOG(LogChanneld, Warning, TEXT("No message handler registered for type: %d"), MessagePack.msgtype());
					continue;
				}
			}
			else
			{
				Entry = MessageHandlers[MsgType];
			}

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
		OnDisconnected();
		// Handle disconnection or exception
		UE_LOG(LogChanneld, Warning, TEXT("Failed to receive data from channeld"));
	}

	// Reset read position
	ReceiveBufferOffset = 0;
}

bool UChanneldConnection::StartReceiveThread()
{
	if (bReceiveThreadRunning)
	{
		return false;
	}
	if (ReceiveThread == nullptr)
	{
		ReceiveThread = FRunnableThread::Create(this, TEXT("Tpri_Channeld_Connection_Receive"));
	}
	return ReceiveThread != nullptr;
}

void UChanneldConnection::StopReceiveThread()
{
	Stop();
	if (ReceiveThread)
	{
		delete ReceiveThread;
		ReceiveThread = nullptr;
	}
}

bool UChanneldConnection::Init()
{
	bReceiveThreadRunning = true;
	return true;
}

uint32 UChanneldConnection::Run()
{
	while (bReceiveThreadRunning)
	{
		Receive();
	}
	return 0;
}

void UChanneldConnection::Stop()
{
	bReceiveThreadRunning = false;
}

void UChanneldConnection::Exit()
{
	if (bReceiveThreadRunning != false)
		bReceiveThreadRunning = true;
}

uint32 UChanneldConnection::AddRpcCallback(const FChanneldMessageHandlerFunc& HandlerFunc)
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
		delete Entry.Msg;
		Entry.Msg = nullptr;
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
	TSharedPtr<channeldpb::MessagePack> MessagePack;
	while (OutgoingQueue.Dequeue(MessagePack))
	{
		Size += MessagePack->ByteSizeLong();
		if (Size >= MaxPacketSize)
			break;
		Packet.add_messages()->CopyFrom(*MessagePack);
	}

	int PacketSize = Packet.ByteSizeLong();
	Size = PacketSize + HeaderSize;
	// TODO: Use a send buffer for all transmissions instead of temp buffer for each transmission
	uint8* PacketData = new uint8[Size];
	if (!Packet.SerializeToArray(PacketData + 5, Size))
	{
		Packet.Clear();
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
	Packet.Clear();
	delete[] PacketData;
	if (!IsSent)
	{
		UE_LOG(LogChanneld, Error, TEXT("Failed to send packet to channeld, size: %d"), Size);
	}
}

void UChanneldConnection::Send(ChannelId ChId, uint32 MsgType, google::protobuf::Message& Msg, channeldpb::BroadcastType Broadcast/* = channeldpb::NO_BROADCAST*/, const FChanneldMessageHandlerFunc& HandlerFunc/* = nullptr*/)
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

	SendRaw(ChId, MsgType, MessageData, Msg.GetCachedSize(), Broadcast, HandlerFunc);

	if (MsgType < channeldpb::USER_SPACE_START)
		UE_LOG(LogChanneld, Verbose, TEXT("Send message %s to channel %d"), channeldpb::MessageType_Name((channeldpb::MessageType)MsgType).c_str(), ChId);
}

void UChanneldConnection::SendRaw(ChannelId ChId, uint32 MsgType, const uint8* MsgBody, const int32 BodySize, channeldpb::BroadcastType Broadcast /*= channeldpb::NO_BROADCAST*/, const FChanneldMessageHandlerFunc& HandlerFunc /*= nullptr*/)
{
	uint32 StubId = HandlerFunc != nullptr ? AddRpcCallback(HandlerFunc) : 0;

	TSharedPtr<channeldpb::MessagePack> MsgPack(new channeldpb::MessagePack);
	MsgPack->set_channelid(ChId);
	MsgPack->set_broadcast(Broadcast);
	MsgPack->set_stubid(StubId);
	MsgPack->set_msgtype(MsgType);
	MsgPack->set_msgbody(MsgBody, BodySize);
	OutgoingQueue.Enqueue(MsgPack);

	/*
	channeldpb::MessagePack MsgPack;
	MsgPack.set_channelid(ChId);
	MsgPack.set_broadcast(Broadcast);
	MsgPack.set_stubid(StubId);
	MsgPack.set_msgtype(MsgType);
	MsgPack.set_msgbody(MsgBody, BodySize);
	OutgoingQueue.Enqueue(MsgPack);
	*/

	if (MsgType >= channeldpb::USER_SPACE_START && bShowUserSpaceMessageLog)
		UE_LOG(LogChanneld, Verbose, TEXT("Send user-space message to channel %d, stubId=%d, type=%d, bodySize=%d)"), ChId, StubId, MsgType, BodySize);
}

void UChanneldConnection::HandleServerForwardMessage(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto UserSpaceMsg = static_cast<const channeldpb::ServerForwardMessage*>(Msg);
	if (!OnUserSpaceMessageReceived.IsBound())
	{
		UE_LOG(LogChanneld, Warning, TEXT("No handler for user-space message, channelId=%d, client connId=%d"), ChId, UserSpaceMsg->clientconnid());
		return;
	}
	OnUserSpaceMessageReceived.Broadcast(ChId, UserSpaceMsg->clientconnid(), UserSpaceMsg->payload());
}

template <typename MsgClass>
FChanneldMessageHandlerFunc WrapMessageHandler(const TFunction<void(const MsgClass*)>& Callback)
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

void UChanneldConnection::RemoveChannel(uint32 ChannelToRemove, const TFunction<void(const channeldpb::RemoveChannelMessage*)>& Callback)
{
	channeldpb::RemoveChannelMessage Msg;
	Msg.set_channelid(ChannelToRemove);
	Send(0, channeldpb::REMOVE_CHANNEL, Msg, channeldpb::NO_BROADCAST, WrapMessageHandler(Callback));
}

void UChanneldConnection::ListChannel(channeldpb::ChannelType TypeFilter /*= channeldpb::UNKNOWN*/, const TArray<FString>* MetadataFilters /*= null*/, const TFunction<void(const channeldpb::ListChannelResultMessage*)>& Callback /*= null*/)
{
	channeldpb::ListChannelMessage ListMsg;
	ListMsg.set_typefilter(TypeFilter);
	if (MetadataFilters != nullptr)
	{
		for (const FString& MetadataFilter : *MetadataFilters)
		{
			ListMsg.add_metadatafilters(TCHAR_TO_ANSI(*MetadataFilter));
		}
	}
	Send(GlobalChannelId, channeldpb::LIST_CHANNEL, ListMsg, channeldpb::NO_BROADCAST, WrapMessageHandler(Callback));
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

void UChanneldConnection::UnsubFromChannel(ChannelId ChId, const TFunction<void(const channeldpb::UnsubscribedFromChannelResultMessage*)>& Callback /*= nullptr*/)
{
	UnsubConnectionFromChannel(GetConnId(), ChId, Callback);
}

void UChanneldConnection::UnsubConnectionFromChannel(ConnectionId TargetConnId, ChannelId ChId, const TFunction<void(const channeldpb::UnsubscribedFromChannelResultMessage*)>& Callback /*= nullptr*/)
{
	channeldpb::UnsubscribedFromChannelMessage Msg;
	Msg.set_connid(TargetConnId);

	Send(ChId, channeldpb::UNSUB_FROM_CHANNEL, Msg, channeldpb::NO_BROADCAST, WrapMessageHandler(Callback));
}

void UChanneldConnection::HandleAuth(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto ResultMsg = static_cast<const channeldpb::AuthResultMessage*>(Msg);
	if (ResultMsg->result() == channeldpb::AuthResultMessage_AuthResult_SUCCESSFUL)
	{
		// Unauthorized yet
		if (ConnId == 0)
		{
			ConnId = ResultMsg->connid();
			CompressionType = ResultMsg->compressiontype();
			OnAuthenticated.Broadcast(this);
		}
		else
		{
			// The master server receives other connection's AuthResultMessage
		}
	}
	else
	{
		UE_LOG(LogChanneld, Error, TEXT("Failed to get authorized by channeld"));
	}
}

void UChanneldConnection::HandleCreateChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto ResultMsg = static_cast<const channeldpb::CreateChannelResultMessage*>(Msg);
	if (ResultMsg->ownerconnid() == GetConnId())
	{
		FOwnedChannelInfo ChannelInfo;
		ChannelInfo.ChannelType = static_cast<EChanneldChannelType>(ResultMsg->channeltype());
		ChannelInfo.ChannelId = ResultMsg->channelid();
		ChannelInfo.Metadata = FString(UTF8_TO_TCHAR(ResultMsg->metadata().c_str()));
		ChannelInfo.OwnerConnId = ResultMsg->ownerconnid();
		OwnedChannels.Add(ResultMsg->channelid(), ChannelInfo);
	}
}

void UChanneldConnection::HandleRemoveChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto RemoveMsg = static_cast<const channeldpb::RemoveChannelMessage*>(Msg);
	SubscribedChannels.Remove(RemoveMsg->channelid());
	OwnedChannels.Remove(RemoveMsg->channelid());
	ListedChannels.Remove(RemoveMsg->channelid());
}

void UChanneldConnection::HandleListChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto ResultMsg = static_cast<const channeldpb::ListChannelResultMessage*>(Msg);
	for (auto ChannelInfo : ResultMsg->channels())
	{
		FListedChannelInfo ListedChannelInfo;
		ListedChannelInfo.ChannelId = ChannelInfo.channelid();
		ListedChannelInfo.ChannelType = static_cast<EChanneldChannelType>(ChannelInfo.channeltype());
		ListedChannelInfo.Metadata = FString(UTF8_TO_TCHAR(ChannelInfo.metadata().c_str()));
		ListedChannels.Add(ChannelInfo.channelid(), ListedChannelInfo);
	}
}

void UChanneldConnection::HandleSubToChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto SubMsg = static_cast<const channeldpb::SubscribedToChannelResultMessage*>(Msg);
	if (SubMsg->connid() == Conn->GetConnId())
	{
		FSubscribedChannelInfo* ExistingSub = SubscribedChannels.Find(ChId);
		if (ExistingSub != nullptr)
		{
			// Merge the SubOptions if the subscription already exists
			ExistingSub->Merge(*SubMsg);
		}
		else
		{
			FSubscribedChannelInfo SubscribedInfo;
			SubscribedInfo.Merge(*SubMsg);
			SubscribedChannels.Add(ChId, SubscribedInfo);
		}
	}
	else
	{
		// Other than channel owner
		FOwnedChannelInfo* ExistingOwnedChannel = OwnedChannels.Find(ChId);
		if (ensureMsgf(ExistingOwnedChannel != nullptr, TEXT("Received other connnection's SubscribedToChannelResultMessage while not owning the channel, Channel ID: %d"), ChId))
		{
			FSubscribedChannelInfo SubscribedInfo;
			SubscribedInfo.Merge(*SubMsg);
			ExistingOwnedChannel->Subscribeds.Add(SubMsg->connid(), SubscribedInfo);
		}
	}
}

void UChanneldConnection::HandleUnsubFromChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto UnsubMsg = static_cast<const channeldpb::UnsubscribedFromChannelResultMessage*>(Msg);
	if (UnsubMsg->connid() == Conn->GetConnId())
	{
		SubscribedChannels.Remove(ChId);
	}
	else
	{
		// Other than channel owner
		FOwnedChannelInfo* ExistingOwnedChannel = OwnedChannels.Find(ChId);
		if (ensureMsgf(ExistingOwnedChannel != nullptr, TEXT("Received other connnection's SubscribedToChannelResultMessage while not owning the channel, Channel ID: %d"), ChId))
		{
			ExistingOwnedChannel->Subscribeds.Remove(UnsubMsg->connid());
		}
	}
}

void UChanneldConnection::HandleChannelDataUpdate(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{

}

