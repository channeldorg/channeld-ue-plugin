// Fill out your copyright notice in the Description page of Project Settings.


#include "ChanneldNetDriver.h"

#include "Net/DataReplication.h"
#include "Net/RepLayout.h"
#include "Misc/ScopeExit.h"
#include "google/protobuf/message_lite.h"

UChanneldNetDriver::UChanneldNetDriver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ChanneldIpForServer = "127.0.0.1";
	ChanneldPortForServer = 11288;
	ChanneldIpForClient = "127.0.0.1";
	ChanneldPortForClient = 12108;
	
	Connection = NewObject<UChanneldConnection>();

	TestChannelData = new testpb::TestChannelDataMessage();

	Connection->AddMessageHandler((uint32)channeldpb::AUTH, this, &UChanneldNetDriver::HandleAuthResult);
	Connection->AddMessageHandler((uint32)channeldpb::CHANNEL_DATA_UPDATE, this, &UChanneldNetDriver::HandleChannelDataUpdate);
}

void UChanneldNetDriver::PostInitProperties()
{
	Super::PostInitProperties();
}

void UChanneldNetDriver::Shutdown()
{
	Super::Shutdown();

	Connection->Disconnect();
}

bool UChanneldNetDriver::IsAvailable() const
{
	return Super::IsAvailable();
}

bool UChanneldNetDriver::InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL,
	bool bReuseAddressAndPort, FString& Error)
{
	Connection->InitSocket(GetSocketSubsystem());
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

	if (Connection->Connect(bInitAsClient, Host, Port, Error))
	{
		/*
		channeldpb::AuthMessage AuthMsg;
		AuthMsg.set_playeridentifiertoken("test_pit");
		AuthMsg.set_logintoken("test_lt");
		Connection->Send(0, channeldpb::AUTH, AuthMsg);
		*/
		Connection->Auth(TEXT("test_pit"), TEXT("test_lt"));
	}
	else
	{
		Error = TEXT("Failed to connect to channeld");
		return false;
	}

	return Super::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error);
}

bool UChanneldNetDriver::InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error)
{
	//return true; 
	return Super::InitConnect(InNotify, ConnectURL, Error);
}

bool UChanneldNetDriver::InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error)
{
	//return true; 
	return Super::InitListen(InNotify, LocalURL, bReuseAddressAndPort, Error);
}

ISocketSubsystem* UChanneldNetDriver::GetSocketSubsystem()
{
	return Super::GetSocketSubsystem();
}


FUniqueSocket UChanneldNetDriver::CreateSocketForProtocol(const FName& ProtocolType)
{
	ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();

	if (SocketSubsystem == NULL)
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

	/* Make sure to cleanly destroy any sockets we do not mean to use. */
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
	return Super::GetSocket();
	// TODO: return the Socket to channeld

	// SetSocket can't be overriden
}

void UChanneldNetDriver::LowLevelSend(TSharedPtr<const FInternetAddr> Address, void* Data, int32 CountBits,
	FOutPacketTraits& Traits)
{
	return Super::LowLevelSend(Address, Data, CountBits, Traits);
}

void UChanneldNetDriver::LowLevelDestroy()
{
	return Super::LowLevelDestroy();
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
		Connection->Send(0, channeldpb::CHANNEL_DATA_UPDATE, UpdateMsg);
	}

	return Result;
}


void UChanneldNetDriver::TickDispatch(float DeltaTime)
{
	Super::TickDispatch(DeltaTime);

	if (Connection->IsConnected())
		Connection->TickIncoming();

/*

	for (const auto ObjRep : AllOwnedReplicators)
	{
		ObjRep->RepState->GetSendingRepState()->RepChangedPropertyTracker->Parents;
	}

	for (const auto& Pair : ReplicationChangeListMap)
	{
		const auto RepLayout = GetObjectClassRepLayout(Pair.Key->GetClass());
		const auto State = Pair.Value->GetRepChangelistState();
		const auto History = State->ChangeHistory[State->HistoryEnd];
		for (const auto CmdIndex : History.Changed)
		{
			const auto Property = RepLayout->Cmds[CmdIndex].Property;
			
		}

		const auto Properties = Pair.Value->GetRepChangelistState()->SharedSerialization.SharedPropertyInfo;
		for (const auto& Property : Properties)
		{
			Property.PropertyKey.ToDebugString();
		}
	}
*/
}

void UChanneldNetDriver::TickFlush(float DeltaSeconds)
{
	Super::TickFlush(DeltaSeconds);

	if (Connection->IsConnected())
		Connection->TickOutgoing();
}

void UChanneldNetDriver::HandleAuthResult(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto AuthResultMsg = static_cast<const channeldpb::AuthResultMessage*>(Msg);
	if (AuthResultMsg->result() == channeldpb::AuthResultMessage_AuthResult_SUCCESSFUL)
	{
		if (IsServer())
		{
			Connection->CreateChannel(channeldpb::GLOBAL, TEXT("test123"), nullptr, nullptr, nullptr,
				[&,ChId](const channeldpb::CreateChannelResultMessage* ResultMsg)
				{
					UE_LOG(LogChanneld, Log, TEXT("[%s] Created channel: %d, type: %s, owner connId: %d"), *GetWorld()->GetDebugDisplayName(),
						ChId, channeldpb::ChannelType_Name(ResultMsg->channeltype()).c_str(), ResultMsg->ownerconnid());

					for (auto const Provider : ChannelDataProviders)
					{
						if (Provider->GetChannelType() == ResultMsg->channeltype())
						{
							Provider->SetChannelId(ChId);
						}
					}
				});
		}
		else
		{
			Connection->SubToChannel(GlobalChannelId, nullptr, [&, ChId](const channeldpb::SubscribedToChannelResultMessage* Msg)
				{
					UE_LOG(LogChanneld, Log, TEXT("[%s] Sub to channel: %d, connId: %d"), *GetWorld()->GetDebugDisplayName(), ChId, Msg->connid());
				});
		}

		UE_LOG(LogChanneld, Log, TEXT("[%s] Successed to get authorization by channeld, connId: %d"), *GetWorld()->GetDebugDisplayName(), Connection->GetConnId());
	}
	else
	{
		UE_LOG(LogChanneld, Error, TEXT("[%s] Failed to get authorization by channeld"), *GetWorld()->GetDebugDisplayName());
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


