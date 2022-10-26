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
#include "ChanneldSettings.h"
#include "Metrics.h"
#include "GameFramework/PlayerState.h"
#include "Replication/ChanneldReplicationComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "ChanneldNetConnection.h"
#include "Engine/World.h"

UChanneldNetDriver::UChanneldNetDriver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UChanneldNetConnection* UChanneldNetDriver::OnClientConnected(ConnectionId ClientConnId)
{
	auto ClientConnection = NewObject<UChanneldNetConnection>(GetTransientPackage(), NetConnectionClass);
	ClientConnection->bDisableHandshaking = GetMutableDefault<UChanneldSettings>()->bDisableHandshaking;
	// Server always sees a connected client (forwarded from channeld) as authenticated.
	ClientConnection->bChanneldAuthenticated = true;
	ClientConnection->InitRemoteConnection(this, GetSocket(), InitBaseURL, ConnIdToAddr(ClientConnId).Get(), EConnectionState::USOCK_Open);

	Notify->NotifyAcceptedConnection(ClientConnection);
	AddClientConnection(ClientConnection);

	ClientConnectionMap.Add(ClientConnId, ClientConnection);

	UE_LOG(LogChanneld, Log, TEXT("Server added client connection %d, total connections: %d (%d)"), ClientConnId, ClientConnections.Num(), ClientConnectionMap.Num());

	if (!ClientConnection->bDisableHandshaking && ConnectionlessHandler.IsValid() && StatelessConnectComponent.IsValid())
	{
		ClientConnection->bInConnectionlessHandshake = true;
	}
	return ClientConnection;
}

void UChanneldNetDriver::OnUserSpaceMessageReceived(uint32 MsgType, ChannelId ChId, ConnectionId ClientConnId, const std::string& Payload)
{
	if (MsgType == MessageType_LOW_LEVEL)
	{
		if (Payload.size() == 0)
		{
			UE_LOG(LogChanneld, Warning, TEXT("Empty payload for LowLeveSend, ClientConnId: %d"), ClientConnId);
			return;
		}
		
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
		auto RpcMsg = MakeShared<unrealpb::RemoteFunctionMessage>();
		if (!RpcMsg->ParseFromString(Payload))
		{
			UE_LOG(LogChanneld, Error, TEXT("Failed to parse RemoteFunctionMessage"));
			return;
		}

		HandleCustomRPC(RpcMsg);
		return;
	}
	else if (MsgType == MessageType_SPAWN)
	{
		auto SpawnMsg = MakeShared<unrealpb::SpawnObjectMessage>();
		if (!SpawnMsg->ParseFromString(Payload))
		{
			UE_LOG(LogChanneld, Error, TEXT("Failed to parse SpawnObjectMessage"));
			return;
		}

		UObject* SpawnedObj = ChanneldUtils::GetObjectByRef(&SpawnMsg->obj(), GetWorld());
		if (SpawnedObj)
		{
			UE_LOG(LogChanneld, Verbose, TEXT("[Client] Spawned object from message: %s, NetGUID: %d, local role: %d"), *SpawnedObj->GetName(), SpawnMsg->obj().netguid(), SpawnMsg->localrole());
			if (SpawnMsg->has_localrole() && SpawnedObj->IsA<AActor>())
			{
				Cast<AActor>(SpawnedObj)->SetRole((ENetRole)SpawnMsg->localrole());
			}
		}
		else
		{
			UE_LOG(LogChanneld, Warning, TEXT("Failed to spawn object from msg: %s"), UTF8_TO_TCHAR(SpawnMsg->DebugString().c_str()));
		}
	}
}

void UChanneldNetDriver::HandleCustomRPC(TSharedPtr<unrealpb::RemoteFunctionMessage> Msg)
{
	AActor* Actor = Cast<AActor>(ChanneldUtils::GetObjectByRef(&Msg->targetobj(), GetWorld()));
	if (!Actor)
	{
		UE_LOG(LogChanneld, Warning, TEXT("Cannot find actor to call remote function '%s', NetGUID: %d. Pushed to the next tick."), UTF8_TO_TCHAR(Msg->functionname().c_str()), Msg->targetobj().netguid());
		UnprocessedRPCs.Add(Msg);
		return;
	}
	//if (!Actor->HasActorBegunPlay())
	//{
	//	UE_LOG(LogChanneld, Warning, TEXT("Actor hasn't begun play to call '%s::%s', NetGUID: %d. Pushed to the next tick."), *Actor->GetName(), UTF8_TO_TCHAR(Msg->functionname().c_str()), Msg->targetobj().netguid());
	//	UnprocessedRPCs.Add(Msg);
	//	return;
	//}

	//TSet<FNetworkGUID> UnmappedGUID;
	bool bDelayRPC = false;
	FName FuncName = UTF8_TO_TCHAR(Msg->functionname().c_str());
	ReceivedRPC(Actor, FuncName, Msg->paramspayload(), bDelayRPC);
	if (bDelayRPC)
	{
		UE_LOG(LogChanneld, Log, TEXT("Delayed RPC '%s::%s' due to unmapped NetGUID."), *Actor->GetName(), *FuncName.ToString());
		UnprocessedRPCs.Add(Msg);
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

			ClientConnectionMap.Remove(ResultMsg->connid());
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

	ConnToChanneld = GEngine->GetEngineSubsystem<UChanneldConnection>();

	ConnToChanneld->OnUserSpaceMessageReceived.AddUObject(this, &UChanneldNetDriver::OnUserSpaceMessageReceived);
	//ConnToChanneld->RegisterMessageHandler(MessageType_LOW_LEVEL, new channeldpb::ServerForwardMessage, this, &UChanneldNetDriver::HandleLowLevelMessage);
	//ConnToChanneld->RegisterMessageHandler(MessageType_RPC, new unrealpb::RemoteFunctionMessage, this, &UChanneldNetDriver::HandleRemoteFunctionMessage);

	InitBaseURL = URL;

	if (!ConnToChanneld->IsConnected())
	{
		FString Host;
		int Port;
		auto Settings = GetMutableDefault<UChanneldSettings>();
		if (bInitAsClient)
		{
			Host = Settings->ChanneldIpForClient;
			Port = Settings->ChanneldPortForClient;
		}
		else
		{
			Host = Settings->ChanneldIpForServer;
			Port = Settings->ChanneldPortForServer;
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

	NetConnection->bDisableHandshaking = GetMutableDefault<UChanneldSettings>()->bDisableHandshaking;
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

	if (!GetMutableDefault<UChanneldSettings>()->bDisableHandshaking)
	{
		InitConnectionlessHandler();
	}

	ConnToChanneld->AddMessageHandler(channeldpb::UNSUB_FROM_CHANNEL, this, &UChanneldNetDriver::ServerHandleUnsub);

	if (!GetMutableDefault<UChanneldSettings>()->bSkipCustomReplication)
	{
		FGameModeEvents::GameModePostLoginEvent.AddUObject(this, &UChanneldNetDriver::OnClientPostLogin);
	}

	GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UChanneldNetDriver::OnServerSpawnedActor));

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

		if (!GetMutableDefault<UChanneldSettings>()->bDisableHandshaking && ConnectionlessHandler.IsValid())
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
	ConnToChanneld->Send(LowLevelSendToChannelId.Get(), MsgType, ServerForwardMessage, channeldpb::SINGLE_CONNECTION);
}

void UChanneldNetDriver::SendDataToClient(uint32 MsgType, ConnectionId ClientConnId, const google::protobuf::Message& Msg)
{
	channeldpb::ServerForwardMessage ServerForwardMessage;
	ServerForwardMessage.set_clientconnid(ClientConnId);
	ServerForwardMessage.set_payload(Msg.SerializeAsString());
	ConnToChanneld->Send(LowLevelSendToChannelId.Get(), MsgType, ServerForwardMessage, channeldpb::SINGLE_CONNECTION);
}

void UChanneldNetDriver::SendDataToServer(uint32 MsgType, uint8* DataToSend, int32 DataSize)
{
	ConnToChanneld->SendRaw(LowLevelSendToChannelId.Get(), MsgType, DataToSend, DataSize);
}

void UChanneldNetDriver::LowLevelDestroy()
{
	Super::LowLevelDestroy();

	FGameModeEvents::GameModePostLoginEvent.RemoveAll(this);

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

void UChanneldNetDriver::OnServerSpawnedActor(AActor* Actor)
{
	if (!Actor->GetIsReplicated())
	{
		return;
	}

	unrealpb::SpawnObjectMessage SpawnMsg;
	// Send the spawning to the clients
	for (auto& Pair : ClientConnectionMap)
	{
		SpawnMsg.mutable_obj()->CopyFrom(ChanneldUtils::GetRefOfObject(Actor, Pair.Value));
		SendDataToClient(MessageType_SPAWN, Pair.Key, SpawnMsg);
	}
}

void UChanneldNetDriver::NotifyActorChannelOpen(UActorChannel* Channel, AActor* Actor)
{
	//if (Actor->IsA<APlayerState>() && Channel->Connection)
	//{
	//	Channel->Connection->SetInternalAck(true);
	//}
	if (auto Comp = Actor->GetComponentByClass(UChanneldReplicationComponent::StaticClass()))
	{
		auto RepComp = Cast<UChanneldReplicationComponent>(Comp);
		RepComp->InitOnce();
	}
}

void UChanneldNetDriver::OnClientPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	auto NewPlayerConn = Cast<UChanneldNetConnection>(NewPlayer->GetNetConnection());
	if (NewPlayerConn == nullptr)
	{
		UE_LOG(LogChanneld, Error, TEXT("PlayerController doesn't have the UChanneldNetConnection. Failed to spawn the objects in the clients."));
		return;
	}

	// Send the GameStateBase to the new player
	auto Comp = Cast<UChanneldReplicationComponent>(NewPlayer->GetComponentByClass(UChanneldReplicationComponent::StaticClass()));
	if (Comp)
	{
		unrealpb::SpawnObjectMessage SpawnMsg;
		SpawnMsg.mutable_obj()->MergeFrom(ChanneldUtils::GetRefOfObject(GameMode->GameState, NewPlayer->GetNetConnection()));
		SendDataToClient(MessageType_SPAWN, NewPlayerConn->GetConnId(), SpawnMsg);
		//ConnToChanneld->Send(Comp->GetChannelId(), MessageType_SPAWN, SpawnMsg);
	}
	else
	{
		UE_LOG(LogChanneld, Warning, TEXT("PlayerController is missing UChanneldReplicationComponent. Failed to spawn the GameStateBase in the client."));
	}

	/* OnServerSpawnedActor handles this
	// By now, the new player's pawn isn't set yet, so we should wait the event
	NewPlayer->GetOnNewPawnNotifier().AddLambda([&, NewPlayerConn](APawn* NewPawn)
		{
			unrealpb::SpawnObjectMessage SpawnPlayerMsg;
			// Send the spawning of the new player's pawn to other clients
			for (auto& Pair : ClientConnectionMap)
			{
				if (Pair.Value != NewPlayerConn)
				{
					SpawnPlayerMsg.mutable_obj()->CopyFrom(ChanneldUtils::GetRefOfObject(NewPawn, Pair.Value));
					SpawnPlayerMsg.set_localrole(ENetRole::ROLE_SimulatedProxy);
					SendDataToClient(MessageType_SPAWN, Pair.Key, SpawnPlayerMsg);
				}
			}
		});
	*/

	// Send the existing player pawns to the new player
	unrealpb::SpawnObjectMessage SpawnPlayerMsg;
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PC = Iterator->Get();
		if (PC && PC != NewPlayer && PC->GetPawn())
		{
			SpawnPlayerMsg.mutable_obj()->CopyFrom(ChanneldUtils::GetRefOfObject(PC->GetPawn(), NewPlayerConn));
			SpawnPlayerMsg.set_localrole(ENetRole::ROLE_SimulatedProxy);
			SendDataToClient(MessageType_SPAWN, NewPlayerConn->GetConnId(), SpawnPlayerMsg);
		}
	}
}

void UChanneldNetDriver::TickDispatch(float DeltaTime)
{
	//Super::TickDispatch(DeltaTime);
	UNetDriver::TickDispatch(DeltaTime);

	if (IsValid(ConnToChanneld) && ConnToChanneld->IsConnected())
		ConnToChanneld->TickIncoming();

	int NumToProcess = UnprocessedRPCs.Num();
	for (int i = 0; i < NumToProcess; i++)
	{
		TSharedPtr<unrealpb::RemoteFunctionMessage> RpcMsg = *UnprocessedRPCs.GetData();
		UnprocessedRPCs.RemoveAt(0);
		HandleCustomRPC(RpcMsg);
	}
}

int32 UChanneldNetDriver::ServerReplicateActors(float DeltaSeconds)
{
	int32 Result = 0;

	if (GetMutableDefault<UChanneldSettings>()->bSkipCustomReplication)
	{
		Result = Super::ServerReplicateActors(DeltaSeconds);
	}
	else
	{
		for (int32 i = 0; i < ClientConnections.Num(); i++)
		{
			UChanneldNetConnection* Connection = CastChecked<UChanneldNetConnection>(ClientConnections[i]);

			/*
			auto PackageMap = CastChecked<UPackageMapClient>(Connection->PackageMap);
			for (auto& Pair : GuidCache->ObjectLookup)
			{
				if (!PackageMap->NetGUIDExportCountMap.Contains(Pair.Key))
				{
					unrealpb::SpawnObjectMessage SpawnMsg;
					SpawnMsg.mutable_obj()->MergeFrom(ChanneldUtils::GetRefOfObject(Pair.Value.Object.Get(), Connection));
					SendDataToClient(MessageType_SPAWN, Connection->GetConnId(), SpawnMsg);
				}
			}
			*/

			if (Connection->PlayerController && Connection->PlayerController->GetLocalRole() == ROLE_AutonomousProxy && Connection->PlayerController->GetViewTarget())
			{
				// Trigger ClientMoveResponse RPC
				Connection->PlayerController->SendClientAdjustment();
			}
		}

		auto Subsystem = GetSubsystem();
		if (Subsystem && Subsystem->GetChannelDataView())
		{
			Result = Subsystem->GetChannelDataView()->SendAllChannelUpdates();
		}
	}
	
	return Result;
}

void UChanneldNetDriver::ProcessRemoteFunction(class AActor* Actor, class UFunction* Function, void* Parameters, struct FOutParmRec* OutParms, struct FFrame* Stack, class UObject* SubObject /*= nullptr*/)
{
	UE_LOG(LogChanneld, VeryVerbose, TEXT("Sending RPC %s::%s, SubObject: %s"), *Actor->GetName(), *Function->GetName(), *GetNameSafe(SubObject));
	if (Function->GetFName() == FName("ClientSetHUD"))
	{
		//UE_DEBUG_BREAK();
	}

	if (!GetMutableDefault<UChanneldSettings>()->bSkipCustomRPC)
	{
		auto RepComp = Cast<UChanneldReplicationComponent>(Actor->FindComponentByClass(UChanneldReplicationComponent::StaticClass()));
		if (RepComp)
		{
			bool bSuccess = true;
			auto ParamsMsg = RepComp->SerializeFunctionParams(Actor, Function, Parameters, bSuccess);
			if (bSuccess)
			{
				auto ChanneldConn = GEngine->GetEngineSubsystem<UChanneldConnection>();
				unrealpb::RemoteFunctionMessage RpcMsg;
				RpcMsg.mutable_targetobj()->MergeFrom(ChanneldUtils::GetRefOfObject(Actor));
				auto FuncName = Function->GetName();
				RpcMsg.set_functionname(TCHAR_TO_UTF8(*FuncName), FuncName.Len());
				if (ParamsMsg)
				{
					RpcMsg.set_paramspayload(ParamsMsg->SerializeAsString());
					UE_LOG(LogChanneld, VeryVerbose, TEXT("Serialized RPC parameters to %dB: %s"), RpcMsg.paramspayload().size(), UTF8_TO_TCHAR(ParamsMsg->DebugString().c_str()));
				}
				uint8* Data = new uint8[RpcMsg.ByteSizeLong()];
				if (RpcMsg.SerializeToArray(Data, RpcMsg.GetCachedSize()))
				{
					if (ConnToChanneld->IsClient())
					{
						SendDataToServer(MessageType_RPC, Data, RpcMsg.GetCachedSize());
						OnSentRPC(Actor, FuncName);
						return;
					}
					else
					{
						auto NetConn = Cast<UChanneldNetConnection>(Actor->GetNetConnection());
						if (NetConn)
						{
							SendDataToClient(MessageType_RPC, NetConn->GetConnId(), Data, RpcMsg.GetCachedSize());
							OnSentRPC(Actor, FuncName);
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
					UE_LOG(LogChanneld, Warning, TEXT("Failed to serialize RPC proto message: %s::%s"), *Actor->GetName(), *Function->GetName());
				}
				// TODO: delete Data?
			}
			else
			{
				UE_LOG(LogChanneld, Warning, TEXT("Failed to serialize RPC function params: %s::%s"), *Actor->GetName(), *Function->GetName());
			}
		}
		else
		{
			UE_LOG(LogChanneld, Warning, TEXT("Can't find the ReplicationComponent to serialize RPC: %s::%s"), *Actor->GetName(), *Function->GetName());
		}
	}

	// Fallback to native RPC
	Super::ProcessRemoteFunction(Actor, Function, Parameters, OutParms, Stack, SubObject);
}

void UChanneldNetDriver::OnSentRPC(class AActor* Actor, FString FuncName)
{
	//UE_LOG(LogChanneld, Verbose, TEXT("Sent RPC %s::%s via channeld"), *Actor->GetName(), *FuncName);
	UMetrics* Metrics = GEngine->GetEngineSubsystem<UMetrics>();
	Metrics->AddConnTypeLabel(*Metrics->SentRPCs).Increment();
}

void UChanneldNetDriver::ReceivedRPC(AActor* Actor, const FName& FunctionName, const std::string& ParamsPayload, bool& bDelayRPC)
{
	UE_LOG(LogChanneld, VeryVerbose, TEXT("Received RPC %s::%s"), *Actor->GetName(), *FunctionName.ToString());

	UFunction* Function = Actor->FindFunction(FunctionName);
	check(Function);

	auto RepComp = Cast<UChanneldReplicationComponent>(Actor->FindComponentByClass(UChanneldReplicationComponent::StaticClass()));
	if (RepComp)
	{
		bool bSuccess = true;
		TSharedPtr<void> Params = RepComp->DeserializeFunctionParams(Actor, Function, ParamsPayload, bSuccess, bDelayRPC);
		if (bDelayRPC)
		{
			return;
		}

		if (bSuccess)
		{
			Actor->ProcessEvent(Function, Params.Get());
		}
		else
		{
			UE_LOG(LogChanneld, Warning, TEXT("Failed to deserialize function parameters of RPC %s::%s"), *Actor->GetName(), *FunctionName.ToString());
		}
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

	bool bDelayRPC;
	ReceivedRPC(Actor, UTF8_TO_TCHAR(RpcMsg->functionname().c_str()), RpcMsg->paramspayload(), bDelayRPC);
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
		CLOCK_CYCLES(SendCycles);
		ConnToChanneld->TickOutgoing();
		UNCLOCK_CYCLES(SendCycles);
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
