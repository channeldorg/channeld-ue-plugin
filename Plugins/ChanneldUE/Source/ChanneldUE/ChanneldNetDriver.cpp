// Fill out your copyright notice in the Description page of Project Settings.


#include "ChanneldNetDriver.h"

#include "Net/DataReplication.h"
#include "Net/RepLayout.h"
#include "Misc/ScopeExit.h"
#include "google/protobuf/message_lite.h"
#include "IpConnection.h"
#include "Engine/NetConnection.h"
#include "PacketHandler.h"

UChanneldNetDriver::UChanneldNetDriver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{	
	ConnToChanneld = NewObject<UChanneldConnection>();

	TestChannelData = new testpb::TestChannelDataMessage();

	ConnToChanneld->UserSpaceMessageHandlerFunc = [&](ChannelId ChId, ConnectionId ClientConnId, const std::string& Payload)
	{
		if (ConnToChanneld->IsClient())
		{
			const auto MyServerConnection = GetServerConnection();
			if (MyServerConnection)
			{
				MyServerConnection->ReceivedRawPacket((uint8*)Payload.data(), Payload.size());
			}
			else
			{
				UE_LOG(LogChanneld, Error, TEXT("ServerConnection doesn't exist"));
			}
		}
		else
		{
			auto ClientConnection = ClientConnectionMap.FindRef(ClientConnId);
			if (ClientConnection == nullptr)
			{
				ClientConnection = NewObject<UChanneldNetConnection>(GetTransientPackage(), NetConnectionClass);
				ClientConnection->InitRemoteConnection(this, GetSocket(), InitBaseURL, ConnIdToAddr(ClientConnId), EConnectionState::USOCK_Open);

				/* Copied from SteamSocketNetDriver.cpp
				// Set up the sequence numbers. Valve uses their own, but so do we so to prevent
				// reinventing the wheel, we'll set up our sequence numbers to be some random garbage
				ClientConnection->InitSequence(4, 4);

				// Attempt to start the PacketHandler handshakes (we do not support stateless connect)
				if (ClientConnection->Handler.IsValid())
				{
					ClientConnection->Handler->BeginHandshaking();
				}
				*/

				Notify->NotifyAcceptedConnection(ClientConnection);
				AddClientConnection(ClientConnection);
				
				ClientConnectionMap.Add(ClientConnId, ClientConnection);
			}

			ClientConnection->ReceivedRawPacket((uint8*)Payload.data(), Payload.size());
		}
	};

	ConnToChanneld->OnAuthenticated.AddUObject(this, &UChanneldNetDriver::OnChanneldAuthenticated);
	//ConnToChanneld->AddMessageHandler((uint32)channeldpb::AUTH, this, &UChanneldNetDriver::HandleAuthResult);
	ConnToChanneld->AddMessageHandler((uint32)channeldpb::CHANNEL_DATA_UPDATE, this, &UChanneldNetDriver::HandleChannelDataUpdate);
}


ConnectionId UChanneldNetDriver::AddrToConnId(const FInternetAddr& Addr)
{
	uint32 ConnId;
	Addr.GetIp(ConnId);
	return ConnId;
}

FInternetAddr& UChanneldNetDriver::ConnIdToAddr(ConnectionId ConnId)
{
	auto AddrPtr = CachedAddr.Find(ConnId);
	if (AddrPtr == nullptr)
	{
		auto Addr = GetSocketSubsystem()->CreateInternetAddr();
		Addr->SetIp(ConnId);
		CachedAddr.Add(ConnId, Addr);
		AddrPtr = &Addr;
	}
	return AddrPtr->Get();
}

void UChanneldNetDriver::PostInitProperties()
{
	Super::PostInitProperties();
}

void UChanneldNetDriver::Shutdown()
{
	Super::Shutdown();

	ConnToChanneld->Disconnect();
}

bool UChanneldNetDriver::IsAvailable() const
{
	return Super::IsAvailable();
}

bool UChanneldNetDriver::InitConnectionClass()
{
	NetConnectionClass = UChanneldNetConnection::StaticClass();
	return true;
}

bool UChanneldNetDriver::InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL,
	bool bReuseAddressAndPort, FString& Error)
{
	InitBaseURL = URL;
	FString Host;
	int Port;
	if (bInitAsClient)
	{
		Host = ChanneldIpForClient;
		Port = ChanneldPortForClient;
	}
	else
	{
		Host = ChanneldIpForServer;
		Port = ChanneldPortForServer;
	}

	if (ConnToChanneld->Connect(bInitAsClient, Host, Port, Error))
	{
		ConnToChanneld->Auth(TEXT("test_pit"), TEXT("test_lt"));
	}
	else
	{
		Error = TEXT("Failed to connect to channeld");
		return false;
	}

	return UNetDriver::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error);
}

bool UChanneldNetDriver::InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error)
{
	ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();
	if (SocketSubsystem == nullptr)
	{
		UE_LOG(LogNet, Warning, TEXT("Unable to find socket subsystem"));
		return false;
	}

	if (!InitBase(true, InNotify, ConnectURL, false, Error))
	{
		UE_LOG(LogNet, Warning, TEXT("Failed to init net driver ConnectURL: %s: %s"), *ConnectURL.ToString(), *Error);
		return false;
	}

	// Create new connection.
	ServerConnection = NewObject<UNetConnection>(GetTransientPackage(), NetConnectionClass);
	UIpConnection* IPConnection = CastChecked<UIpConnection>(ServerConnection);

	if (IPConnection == nullptr)
	{
		Error = TEXT("Could not cast the ServerConnection into the base connection class for this netdriver!");
		return false;
	}

	ServerConnection->InitLocalConnection(this, GetSocket(), ConnectURL, USOCK_Open);

	UE_LOG(LogNet, Log, TEXT("Game client on port %i, rate %i"), ConnectURL.Port, ServerConnection->CurrentNetSpeed);
	CreateInitialClientChannels();

	/* IpNetDriver::InitConnect causes exception by using unset Socket 
	bool bResult = Super::InitConnect(InNotify, ConnectURL, Error);

	if (bResult && ServerConnection)
	{
		ServerConnection->State = USOCK_Open;
	}
	*/

	/* Copied from SteamSocketNetDriver.cpp
	// Attempt to start the PacketHandler handshakes (we do not support stateless connect)
	// The PendingNetGame will also do it but we can't actually send the packets for it until we're connected.
	if (ServerConnection->Handler.IsValid())
	{
		ServerConnection->Handler->BeginHandshaking();
	}
	*/

	return true;
}

bool UChanneldNetDriver::InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error)
{
	return InitBase(false, InNotify, LocalURL, bReuseAddressAndPort, Error);
	//return Super::InitListen(InNotify, LocalURL, bReuseAddressAndPort, Error);
}

ISocketSubsystem* UChanneldNetDriver::GetSocketSubsystem()
{
	return Super::GetSocketSubsystem();
}


FUniqueSocket UChanneldNetDriver::CreateSocketForProtocol(const FName& ProtocolType)
{
	ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();
	if (SocketSubsystem == nullptr)
	{
		UE_LOG(LogNet, Warning, TEXT("UChanneldNetDriver::CreateSocket: Unable to find socket subsystem"));
		return NULL;
	}

	return SocketSubsystem->CreateUniqueSocket(NAME_DGram, TEXT("Connection to channeld"), ProtocolType);
}

FUniqueSocket UChanneldNetDriver::CreateAndBindSocket(TSharedRef<FInternetAddr> BindAddr, int32 Port, bool bReuseAddressAndPort, int32 DesiredRecvSize, int32 DesiredSendSize, FString& Error)
{
	ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();
	if (SocketSubsystem == nullptr)
	{
		Error = TEXT("Unable to find socket subsystem");
		return nullptr;
	}

	// Create the socket that we will use to communicate with
	FUniqueSocket NewSocket = CreateSocketForProtocol(BindAddr->GetProtocolType());

	if (!NewSocket.IsValid())
	{
		Error = FString::Printf(TEXT("%s: socket failed (%i)"), SocketSubsystem->GetSocketAPIName(), (int32)SocketSubsystem->GetLastErrorCode());
		return nullptr;
	}

	// Make sure to cleanly destroy any sockets we do not mean to use.
	ON_SCOPE_EXIT
	{
		if (Error.IsEmpty() == false)
		{
			NewSocket.Reset();
		}
	};

	//if (SocketSubsystem->RequiresChatDataBeSeparate() == false && NewSocket->SetBroadcast() == false)
	//{
	//	Error = FString::Printf(TEXT("%s: setsockopt SO_BROADCAST failed (%i)"), SocketSubsystem->GetSocketAPIName(), (int32)SocketSubsystem->GetLastErrorCode());
	//	return nullptr;
	//}

	if (NewSocket->SetReuseAddr(bReuseAddressAndPort) == false)
	{
		UE_LOG(LogNet, Log, TEXT("setsockopt with SO_REUSEADDR failed"));
	}

	if (NewSocket->SetRecvErr() == false)
	{
		UE_LOG(LogNet, Log, TEXT("setsockopt with IP_RECVERR failed"));
	}

	int32 ActualRecvSize(0);
	int32 ActualSendSize(0);
	NewSocket->SetReceiveBufferSize(DesiredRecvSize, ActualRecvSize);
	NewSocket->SetSendBufferSize(DesiredSendSize, ActualSendSize);
	UE_LOG(LogInit, Log, TEXT("%s: Socket queue. Rx: %i (config %i) Tx: %i (config %i)"), SocketSubsystem->GetSocketAPIName(),
		ActualRecvSize, DesiredRecvSize, ActualSendSize, DesiredSendSize);

	// Bind socket to our port.
	BindAddr->SetPort(Port);

	int32 AttemptPort = BindAddr->GetPort();
	int32 BoundPort = SocketSubsystem->BindNextPort(NewSocket.Get(), *BindAddr, MaxPortCountToTry + 1, 1);
	if (BoundPort == 0)
	{
		Error = FString::Printf(TEXT("%s: binding to port %i failed (%i)"), SocketSubsystem->GetSocketAPIName(), AttemptPort,
			(int32)SocketSubsystem->GetLastErrorCode());
		return nullptr;
	}
	if (NewSocket->SetNonBlocking() == false)
	{
		Error = FString::Printf(TEXT("%s: SetNonBlocking failed (%i)"), SocketSubsystem->GetSocketAPIName(),
			(int32)SocketSubsystem->GetLastErrorCode());
		return nullptr;
	}

	return NewSocket;
}

FSocket* UChanneldNetDriver::GetSocket()
{
	//return Super::GetSocket();
	// Return the Socket to channeld
	return ConnToChanneld->GetSocket();

	// SetSocket can't be overridden
}

void UChanneldNetDriver::LowLevelSend(TSharedPtr<const FInternetAddr> Address, void* Data, int32 CountBits,
	FOutPacketTraits& Traits)
{
	//Super::LowLevelSend(Address, Data, CountBits, Traits);

	if (!ConnToChanneld->IsAuthenticated())
	{
		
		LowLevelSendDataBeforeAuth.Enqueue(MakeTuple(Address, new std::string(reinterpret_cast<const char*>(Data), FMath::DivideAndRoundUp(CountBits, 8)), &Traits));
	}
	else
	{
		// Copied from UIpNetDriver::LowLevelSend
		const uint8* DataToSend = reinterpret_cast<uint8*>(Data);
		int32 DataSize = FMath::DivideAndRoundUp(CountBits, 8);

		//int32 BytesSent = 0;
		if (CountBits > 0 && ConnToChanneld->IsConnected())
		{
			if (ConnToChanneld->IsServer())
			{
				ConnectionId ClientConnId = AddrToConnId(*Address);
				channeldpb::ServerForwardMessage ServerForwardMessage;
				ServerForwardMessage.set_clientconnid(ClientConnId);
				ServerForwardMessage.set_payload(Data, DataSize);
				CLOCK_CYCLES(SendCycles);
				ConnToChanneld->Send(LowLevelSendToChannelId, channeldpb::USER_SPACE_START, ServerForwardMessage, channeldpb::SINGLE_CONNECTION);
				UNCLOCK_CYCLES(SendCycles);
			}
			else
			{
				CLOCK_CYCLES(SendCycles);
				ConnToChanneld->SendRaw(LowLevelSendToChannelId, channeldpb::USER_SPACE_START, DataToSend, DataSize);
				UNCLOCK_CYCLES(SendCycles);
			}
		}
	}
	
}

void UChanneldNetDriver::LowLevelDestroy()
{
	ConnToChanneld->Disconnect(true);
	Super::LowLevelDestroy();
}

bool UChanneldNetDriver::IsNetResourceValid()
{
	return Super::IsNetResourceValid();
}

void UChanneldNetDriver::RegisterChannelDataProvider(IChannelDataProvider* Provider)
{
	ChannelDataProviders.Add(Provider);
}

int32 UChanneldNetDriver::ServerReplicateActors(float DeltaSeconds)
{
	auto const Result = Super::ServerReplicateActors(DeltaSeconds);

	int32 Updated = 0;
	for (auto const Provider : ChannelDataProviders)
	{
		Updated += Provider->UpdateChannelData(TestChannelData);
	}

	if (Updated > 0)
	{
		const auto ByteString = TestChannelData->SerializeAsString();
		UE_LOG(LogTemp, Log, TEXT("TestChannelData has %d update(s). Serialized: %s"), Updated, ByteString.c_str());

		// Send to channeld
		channeldpb::ChannelDataUpdateMessage UpdateMsg;
		UpdateMsg.mutable_data()->PackFrom(*TestChannelData);
		ConnToChanneld->Send(0, channeldpb::CHANNEL_DATA_UPDATE, UpdateMsg);
	}

	return Result;
}


void UChanneldNetDriver::TickDispatch(float DeltaTime)
{
	//Super::TickDispatch(DeltaTime);

	if (IsValid(ConnToChanneld) && ConnToChanneld->IsConnected())
		ConnToChanneld->TickIncoming();
}

void UChanneldNetDriver::TickFlush(float DeltaSeconds)
{
	// Trigger the callings of LowLevelSend()
	Super::TickFlush(DeltaSeconds);

	if (IsValid(ConnToChanneld) && ConnToChanneld->IsConnected())
		ConnToChanneld->TickOutgoing();
}

void UChanneldNetDriver::OnChanneldAuthenticated(UChanneldConnection* _)
{
	while (!LowLevelSendDataBeforeAuth.IsEmpty())
	{
		TTuple<TSharedPtr<const FInternetAddr>, std::string*, FOutPacketTraits*> Params;
		LowLevelSendDataBeforeAuth.Dequeue(Params);
		std::string* data = Params.Get<1>();
		LowLevelSend(Params.Get<0>(), (uint8*)data->data(), data->size()*8, *Params.Get<2>());
		delete data;
	}

	if (ConnToChanneld->IsServer())
	{
		ConnToChanneld->CreateChannel(channeldpb::GLOBAL, TEXT("test123"), nullptr, nullptr, nullptr,
			[&](const channeldpb::CreateChannelResultMessage* ResultMsg)
			{
				UE_LOG(LogChanneld, Log, TEXT("[%s] Created channel: %d, type: %s, owner connId: %d, metadata: %s"), *GetWorld()->GetDebugDisplayName(),
					ResultMsg->channelid(), channeldpb::ChannelType_Name(ResultMsg->channeltype()).c_str(), ResultMsg->ownerconnid(), ResultMsg->metadata().c_str());

				for (auto const Provider : ChannelDataProviders)
				{
					if (Provider->GetChannelType() == ResultMsg->channeltype())
					{
						Provider->SetChannelId(ResultMsg->channelid());
					}
				}
			});
	}
	else
	{
		ChannelId ChIdToSub = GlobalChannelId;
		ConnToChanneld->SubToChannel(ChIdToSub, nullptr, [&, ChIdToSub](const channeldpb::SubscribedToChannelResultMessage* Msg)
			{
				UE_LOG(LogChanneld, Log, TEXT("[%s] Sub to channel: %d, connId: %d"), *GetWorld()->GetDebugDisplayName(), ChIdToSub, Msg->connid());
			});
	}

}

void UChanneldNetDriver::HandleChannelDataUpdate(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto UpdateMsg = static_cast<const channeldpb::ChannelDataUpdateMessage*>(Msg);
	for (auto const Provider : ChannelDataProviders)
	{
		if (Provider->GetChannelId() == ChId)
		{
			Provider->OnChannelDataUpdated(UpdateMsg);
		}
	}
}


