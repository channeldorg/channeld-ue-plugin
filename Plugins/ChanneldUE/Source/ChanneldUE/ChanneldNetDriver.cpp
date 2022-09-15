// Fill out your copyright notice in the Description page of Project Settings.


#include "ChanneldNetDriver.h"

#include "Net/DataReplication.h"
#include "Net/RepLayout.h"
#include "Misc/ScopeExit.h"
#include "google/protobuf/message_lite.h"
#include "Engine/NetConnection.h"
#include "PacketHandler.h"
#include "Net/Core/Misc/PacketAudit.h"
#include "ChanneldGameInstanceSubsystem.h"
#include "Replication/ChanneldReplicationDriver.h"
#include "ChanneldUtils.h"
#include "Replication/ChanneldReplicationComponent.h"

UChanneldNetDriver::UChanneldNetDriver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UChanneldNetConnection* UChanneldNetDriver::OnClientConnected(ConnectionId ClientConnId)
{
	auto ClientConnection = NewObject<UChanneldNetConnection>(GetTransientPackage(), NetConnectionClass);
	ClientConnection->bDisableHandshaking = bDisableHandshaking;
	// Server always sees a connected client (forwarded from channeld) as authenticated.
	ClientConnection->bChanneldAuthenticated = true;
	ClientConnection->InitRemoteConnection(this, GetSocket(), InitBaseURL, ConnIdToAddr(ClientConnId).Get(), EConnectionState::USOCK_Open);

	Notify->NotifyAcceptedConnection(ClientConnection);
	AddClientConnection(ClientConnection);

	ClientConnectionMap.Add(ClientConnId, ClientConnection);

	UE_LOG(LogChanneld, Log, TEXT("Server added client connection %d, total connections: %d (%d)"), ClientConnId, ClientConnections.Num(), ClientConnectionMap.Num());

	if (!bDisableHandshaking && ConnectionlessHandler.IsValid() && StatelessConnectComponent.IsValid())
	{
		ClientConnection->bInConnectionlessHandshake = true;
	}
	return ClientConnection;
}

void UChanneldNetDriver::OnUserSpaceMessageReceived(uint32 MsgType, ChannelId ChId, ConnectionId ClientConnId, const std::string& Payload)
{
	if (MsgType == MessageType_LOW_LEVEL)
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
			// Server's ClientConnection is created when the first packet from client arrives.
			if (ClientConnection == nullptr)
			{
				ClientConnection = OnClientConnected(ClientConnId);
			}
			ClientConnection->ReceivedRawPacket((uint8*)Payload.data(), Payload.size());
		}
	}
	else if (MsgType == MessageType_RPC)
	{
		unrealpb::RemoteFunctionMessage Msg;
		if (!Msg.ParseFromString(Payload))
		{
			UE_LOG(LogChanneld, Error, TEXT("Failed to parse RemoteFunction message"));
			return;
		}

		AActor* Actor = Cast<AActor>(ChanneldUtils::GetObjectByRef(&Msg.targetobj(), GetWorld()));
		if (!Actor)
		{
			UE_LOG(LogChanneld, Error, TEXT("Cannot find actor to call remote function '%s', NetGUID: %d"), UTF8_TO_TCHAR(Msg.functionname().c_str()), Msg.targetobj().netguid());
			return;
		}

		TSet<FNetworkGUID> UnmappedGUID;
		ReceivedRPC(Actor, UTF8_TO_TCHAR(Msg.functionname().c_str()), Msg.paramspayload(), UnmappedGUID);
		// TODO: Handle unmapped GUIDs?
	}
}

void UChanneldNetDriver::ServerHandleUnsub(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto ResultMsg = static_cast<const channeldpb::UnsubscribedFromChannelResultMessage*>(Msg);
	UE_LOG(LogChanneld, Log, TEXT("Server received unsub of conn(%d), connType=%s, channelType=%s, channelId=%d"),
		ResultMsg->connid(),
		UTF8_TO_TCHAR(channeldpb::ConnectionType_Name(ResultMsg->conntype()).c_str()),
		UTF8_TO_TCHAR(channeldpb::ChannelType_Name(ResultMsg->channeltype()).c_str()),
		ChId);

	if (ResultMsg->conntype() == channeldpb::CLIENT && Conn->OwnedChannels.Contains(ChId))
	{
		auto ClientConnection = ClientConnectionMap.FindRef(ResultMsg->connid());
		if (ClientConnection)
		{
			ClientConnectionMap.Remove(ResultMsg->connid());

			//~ Start copy from UNetDriver::Shutdown()
			if (ClientConnection->PlayerController)
			{
				APawn* Pawn = ClientConnection->PlayerController->GetPawn();
				if (Pawn)
				{
					Pawn->Destroy(true);
				}
			}

			// Calls Close() internally and removes from ClientConnections
			ClientConnection->CleanUp();
			//~ End copy
		}
	}
}

ConnectionId UChanneldNetDriver::AddrToConnId(const FInternetAddr& Addr)
{
	uint32 ConnId;
	Addr.GetIp(ConnId);
	return ConnId;
}

TSharedRef<FInternetAddr> UChanneldNetDriver::ConnIdToAddr(ConnectionId ConnId)
{
	auto AddrPtr = CachedAddr.Find(ConnId);
	if (AddrPtr == nullptr)
	{
		auto Addr = GetSocketSubsystem()->CreateInternetAddr();
		Addr->SetIp(ConnId);
		CachedAddr.Add(ConnId, Addr);
		AddrPtr = &Addr;
	}
	return *AddrPtr;
}

UChanneldGameInstanceSubsystem* UChanneldNetDriver::GetSubsystem()
{
	UWorld* TheWorld = GetWorld();
	if (!TheWorld)
	{
		TheWorld = GEngine->GetWorldContextFromPendingNetGameNetDriver(this)->World();
	}
	auto GameInstance = TheWorld->GetGameInstance();
	if (GameInstance)
	{
		return GameInstance->GetSubsystem<UChanneldGameInstanceSubsystem>();
	}
	return nullptr;
}

void UChanneldNetDriver::PostInitProperties()
{
	Super::PostInitProperties();
}

bool UChanneldNetDriver::IsAvailable() const
{
	return true;
}

bool UChanneldNetDriver::InitConnectionClass()
{
	NetConnectionClass = UChanneldNetConnection::StaticClass();
	return true;
}

bool UChanneldNetDriver::InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL,
	bool bReuseAddressAndPort, FString& Error)
{
	{
		auto Subsystem = GetSubsystem();
		if (Subsystem)
		{
			LowLevelSendToChannelId = Subsystem->LowLevelSendToChannelId;
			/*
			// Share the same ChanneldConnection with the subsystem if it exists.
			// In the standalone net game, NetDriver::InitBase is called when starting client travel, so the connection in the subsystem should be already created.
			ConnToChanneld = Subsystem->GetConnection();
			*/
		}
	}
	/*
	if (ConnToChanneld == nullptr)
	{
		ConnToChanneld = NewObject<UChanneldConnection>(this);
	}
	*/
	ConnToChanneld = GEngine->GetEngineSubsystem<UChanneldConnection>();

	ConnToChanneld->OnUserSpaceMessageReceived.AddUObject(this, &UChanneldNetDriver::OnUserSpaceMessageReceived);
	//ConnToChanneld->RegisterMessageHandler(MessageType_LOW_LEVEL, new channeldpb::ServerForwardMessage, this, &UChanneldNetDriver::HandleLowLevelMessage);
	//ConnToChanneld->RegisterMessageHandler(MessageType_RPC, new unrealpb::RemoteFunctionMessage, this, &UChanneldNetDriver::HandleRemoteFunctionMessage);

	InitBaseURL = URL;

	if (!ConnToChanneld->IsConnected())
	{
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

		if (!ConnToChanneld->Connect(bInitAsClient, Host, Port, Error))
		{
			Error = TEXT("Failed to connect to channeld");
			return false;
		}
	}

	if (!ConnToChanneld->IsAuthenticated())
	{
		ConnToChanneld->OnAuthenticated.AddUObject(this, &UChanneldNetDriver::OnChanneldAuthenticated);

		ConnToChanneld->Auth(TEXT("test_pit"), TEXT("test_lt"));
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

	/* Driver->ConnectionlessHandler is only used in server
	if (!bDisableHandshaking)
	{
		InitConnectionlessHandler();
	}
	*/

	// Create new connection.
	ServerConnection = NewObject<UNetConnection>(GetTransientPackage(), NetConnectionClass);
	UChanneldNetConnection* NetConnection = CastChecked<UChanneldNetConnection>(ServerConnection);

	if (NetConnection == nullptr)
	{
		Error = TEXT("Could not cast the ServerConnection into the base connection class for this netdriver!");
		return false;
	}

	NetConnection->bDisableHandshaking = bDisableHandshaking;
	ServerConnection->InitLocalConnection(this, GetSocket(), ConnectURL, USOCK_Open);
	//NetConnection->bInConnectionlessHandshake = true;

	UE_LOG(LogNet, Log, TEXT("Game client on port %i, rate %i"), ConnectURL.Port, ServerConnection->CurrentNetSpeed);
	CreateInitialClientChannels();

	if (ConnToChanneld->IsAuthenticated())
	{
		// Connection is already authenticated via the subsystem
		OnChanneldAuthenticated(ConnToChanneld);
	}

	return true;
}

bool UChanneldNetDriver::InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error)
{
	if (!InitBase(false, InNotify, LocalURL, bReuseAddressAndPort, Error))
	{
		UE_LOG(LogNet, Warning, TEXT("Failed to init net driver ListenURL: %s: %s"), *LocalURL.ToString(), *Error);
		return false;
	}

	if (!bDisableHandshaking)
	{
		InitConnectionlessHandler();
	}

	ConnToChanneld->AddMessageHandler(channeldpb::UNSUB_FROM_CHANNEL, this, &UChanneldNetDriver::ServerHandleUnsub);

	return true;

	//return Super::InitListen(InNotify, LocalURL, bReuseAddressAndPort, Error);
}

ISocketSubsystem* UChanneldNetDriver::GetSocketSubsystem()
{
	return ISocketSubsystem::Get();
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
	
	if (ConnToChanneld->IsConnected() && Address.IsValid() && Address->IsValid())
	{
		// Copied from UIpNetDriver::LowLevelSend
		uint8* DataToSend = reinterpret_cast<uint8*>(Data);
		int32 DataSize = FMath::DivideAndRoundUp(CountBits, 8);

		FPacketAudit::NotifyLowLevelReceive(DataToSend, DataSize);

		if (!bDisableHandshaking && ConnectionlessHandler.IsValid())
		{
			const ProcessedPacket ProcessedData =
				ConnectionlessHandler->OutgoingConnectionless(Address, (uint8*)DataToSend, CountBits, Traits);

			if (!ProcessedData.bError)
			{
				DataToSend = ProcessedData.Data;
				DataSize = FMath::DivideAndRoundUp(ProcessedData.CountBits, 8);
			}
			else
			{
				return;
			}
		}

		if (ConnToChanneld->IsServer())
		{
			ConnectionId ClientConnId = AddrToConnId(*Address);
			SendDataToClient(MessageType_LOW_LEVEL, ClientConnId, DataToSend, DataSize);
		}
		else
		{
			SendDataToServer(MessageType_LOW_LEVEL, DataToSend, DataSize);
		}
	}
}

void UChanneldNetDriver::SendDataToClient(uint32 MsgType, ConnectionId ClientConnId, uint8* DataToSend, int32 DataSize)
{
	channeldpb::ServerForwardMessage ServerForwardMessage;
	ServerForwardMessage.set_clientconnid(ClientConnId);
	ServerForwardMessage.set_payload(DataToSend, DataSize);
	CLOCK_CYCLES(SendCycles);
	ConnToChanneld->Send(LowLevelSendToChannelId.Get(), MsgType, ServerForwardMessage, channeldpb::SINGLE_CONNECTION);
	UNCLOCK_CYCLES(SendCycles);
}

void UChanneldNetDriver::SendDataToServer(uint32 MsgType, uint8* DataToSend, int32 DataSize)
{
	CLOCK_CYCLES(SendCycles);
	ConnToChanneld->SendRaw(LowLevelSendToChannelId.Get(), MsgType, DataToSend, DataSize);
	UNCLOCK_CYCLES(SendCycles);
}

void UChanneldNetDriver::LowLevelDestroy()
{
	Super::LowLevelDestroy();

	if (ConnToChanneld)
	{
		ConnToChanneld->OnAuthenticated.RemoveAll(this);
		ConnToChanneld->OnUserSpaceMessageReceived.RemoveAll(this);
		ConnToChanneld->RemoveMessageHandler(channeldpb::UNSUB_FROM_CHANNEL, this);
		//ConnToChanneld->RemoveMessageHandler(MessageType_LOW_LEVEL, this);
		//ConnToChanneld->RemoveMessageHandler(MessageType_RPC, this);

		/*
		// Only disconnect when the connection is owned by the net driver.
		// Otherwise the connection is owned by the subsystem, and it will be used across sessions, 
		// while the net driver will be created and destroyed for every client travel.
		if (ConnToChanneld->GetOuter() == this)
		{
			ConnToChanneld->Disconnect(true);
		}
		*/
	}
	
	ClientConnectionMap.Reset();
	
	ConnToChanneld = NULL;
}

bool UChanneldNetDriver::IsNetResourceValid()
{
	if ((ConnToChanneld->IsServer() && !ServerConnection)//  Server
		|| (ConnToChanneld->IsClient() && ServerConnection) // client
		)
	{
		return true;
	}

	return false;
}

void UChanneldNetDriver::TickDispatch(float DeltaTime)
{
	//Super::TickDispatch(DeltaTime);
	UNetDriver::TickDispatch(DeltaTime);

	if (IsValid(ConnToChanneld) && ConnToChanneld->IsConnected())
		ConnToChanneld->TickIncoming();
}

int32 UChanneldNetDriver::ServerReplicateActors(float DeltaSeconds)
{
	int32 Result = Super::ServerReplicateActors(DeltaSeconds);
	//UE_LOG(LogChanneld, Verbose, TEXT("Super::ServerReplicateActors replicated %d actors"), Result);

	if (!bSkipCustomReplication)
	{
		auto Subsystem = GetSubsystem();
		if (Subsystem && Subsystem->GetChannelDataView())
		{
			Result += Subsystem->GetChannelDataView()->SendAllChannelUpdates();
		}
	}
	
	return Result;
}

void UChanneldNetDriver::ProcessRemoteFunction(class AActor* Actor, class UFunction* Function, void* Parameters, struct FOutParmRec* OutParms, struct FFrame* Stack, class UObject* SubObject /*= nullptr*/)
{
	if (!bSkipCustomRPC)
	{
		UE_LOG(LogChanneld, Verbose, TEXT("Send RPC %s::%s, SubObject: %s"), *Actor->GetName(), *Function->GetName(), *GetNameSafe(SubObject));

		auto RepComp = Cast<UChanneldReplicationComponent>(Actor->FindComponentByClass(UChanneldReplicationComponent::StaticClass()));
		if (RepComp)
		{
			auto ParamsMsg = RepComp->SerializeFunctionParams(Actor, Function, Parameters);
			if (ParamsMsg)
			{
				auto ChanneldConn = GEngine->GetEngineSubsystem<UChanneldConnection>();
				unrealpb::RemoteFunctionMessage RpcMsg;
				RpcMsg.mutable_targetobj()->MergeFrom(ChanneldUtils::GetRefOfObject(Actor));
				auto FuncName = Function->GetName();
				RpcMsg.set_functionname(TCHAR_TO_UTF8(*FuncName), FuncName.Len());
				RpcMsg.set_paramspayload(ParamsMsg->SerializeAsString());
				uint8* Data = new uint8[RpcMsg.ByteSizeLong()];
				if (RpcMsg.SerializeToArray(Data, RpcMsg.GetCachedSize()))
				{
					if (ConnToChanneld->IsClient())
					{
						SendDataToServer(MessageType_RPC, Data, RpcMsg.GetCachedSize());
						return;
					}
					else
					{
						auto NetConn = Cast<UChanneldNetConnection>(Actor->GetNetConnection());
						if (NetConn)
						{
							SendDataToClient(MessageType_RPC, NetConn->GetConnId(), Data, RpcMsg.GetCachedSize());
							return;
						}
						else
						{
							UE_LOG(LogChanneld, Warning, TEXT("Failed to send RPC %s::%s as the actor doesn't have any client connection"), *Actor->GetName(), *Function->GetName());
						}
					}
				}
				else
				{
					UE_LOG(LogChanneld, Warning, TEXT("Failed to serialize RPC message"));
				}
				// TODO: delete Data?
			}
		}
	}

	// Fallback to native RPC
	Super::ProcessRemoteFunction(Actor, Function, Parameters, OutParms, Stack, SubObject);
}

void UChanneldNetDriver::ReceivedRPC(AActor* Actor, const FName& FunctionName, const std::string& ParamsPayload, TSet<FNetworkGUID>& UnmappedGuids)
{
	UE_LOG(LogChanneld, Verbose, TEXT("Received RPC %s::%s"), *Actor->GetName(), *FunctionName.ToString());

	UFunction* Function = Actor->FindFunction(FunctionName);
	check(Function);

	auto RepComp = Cast<UChanneldReplicationComponent>(Actor->FindComponentByClass(UChanneldReplicationComponent::StaticClass()));
	if (RepComp)
	{
		void* Params = RepComp->DeserializeFunctionParams(Actor, Function, ParamsPayload);
		Actor->ProcessEvent(Function, Params);
	}
}

void UChanneldNetDriver::HandleRemoteFunctionMessage(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto RpcMsg = static_cast<const unrealpb::RemoteFunctionMessage*>(Msg);

	AActor* Actor = Cast<AActor>(ChanneldUtils::GetObjectByRef(&RpcMsg->targetobj(), GetWorld()));
	if (!Actor)
	{
		UE_LOG(LogChanneld, Error, TEXT("Cannot find actor to call remote function '%s', NetGUID: %d"), UTF8_TO_TCHAR(RpcMsg->functionname().c_str()), RpcMsg->targetobj().netguid());
		return;
	}

	TSet<FNetworkGUID> UnmappedGUID;
	ReceivedRPC(Actor, UTF8_TO_TCHAR(RpcMsg->functionname().c_str()), RpcMsg->paramspayload(), UnmappedGUID);
	// TODO: Handle unmapped GUIDs?
}

void UChanneldNetDriver::HandleLowLevelMessage(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto ServerForwardMsg = static_cast<const channeldpb::ServerForwardMessage*>(Msg);
	if (ConnToChanneld->IsClient())
	{
		const auto MyServerConnection = GetServerConnection();
		if (MyServerConnection)
		{
			MyServerConnection->ReceivedRawPacket((uint8*)ServerForwardMsg->payload().data(), ServerForwardMsg->payload().size());
		}
		else
		{
			UE_LOG(LogChanneld, Error, TEXT("ServerConnection doesn't exist"));
		}
	}
	else
	{
		auto ClientConnection = ClientConnectionMap.FindRef(ServerForwardMsg->clientconnid());
		// Server's ClientConnection is created when the first packet from client arrives.
		if (ClientConnection == nullptr)
		{
			ClientConnection = OnClientConnected(ServerForwardMsg->clientconnid());
		}
		ClientConnection->ReceivedRawPacket((uint8*)ServerForwardMsg->payload().data(), ServerForwardMsg->payload().size());
	}
}


void UChanneldNetDriver::TickFlush(float DeltaSeconds)
{
	// Trigger the callings of LowLevelSend()
	UNetDriver::TickFlush(DeltaSeconds);

	if (ConnToChanneld && ConnToChanneld->IsConnected())
	{
		ConnToChanneld->TickOutgoing();
	}
}

void UChanneldNetDriver::OnChanneldAuthenticated(UChanneldConnection* _)
{
	if (ConnToChanneld->IsClient())
	{
		auto MyServerConnection = GetServerConnection();
		MyServerConnection->RemoteAddr = ConnIdToAddr(ConnToChanneld->GetConnId());
		MyServerConnection->bChanneldAuthenticated = true;
		MyServerConnection->FlushUnauthData();
	}
}
