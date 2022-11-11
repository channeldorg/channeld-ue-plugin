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

UChanneldNetConnection* UChanneldNetDriver::AddChanneldClientConnection(ConnectionId ClientConnId)
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
			// Server's ClientConnection is created when the first packet (NMT_Hello) from client arrives.
			if (ClientConnection == nullptr)
			{
				ClientConnection = AddChanneldClientConnection(ClientConnId);
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

		// If the object with the same NetId exists, destroy it before spawning a new one.
		UObject* OldObj = ChanneldUtils::GetObjectByRef(&SpawnMsg->obj(), GetWorld(), false);
		if (OldObj)
		{
			UE_LOG(LogChanneld, Log, TEXT("Found spawned object %s of duplicated NetId: %d, will be destroyed."), *GetNameSafe(OldObj), SpawnMsg->obj().netguid());
			
			GuidCache->ObjectLookup.Remove(FNetworkGUID(SpawnMsg->obj().netguid()));
			GuidCache->NetGUIDLookup.Remove(OldObj);
			
			if (AActor* OldActor = Cast<AActor>(OldObj))
			{
				GetWorld()->DestroyActor(OldActor);
			}
		}

		if (ChannelDataView.IsValid() && SpawnMsg->has_channelid())
		{
			// Set up the mapping before actually spawn it, so AddProvider() can find the mapping.
			ChannelDataView->SetOwningChannelId(FNetworkGUID(SpawnMsg->obj().netguid()), SpawnMsg->channelid());
		}
		
		UObject* NewObj = ChanneldUtils::GetObjectByRef(&SpawnMsg->obj(), GetWorld());
		if (NewObj)
		{
			UE_LOG(LogChanneld, Verbose, TEXT("[Client] Spawned object from message: %s, NetGUID: %d, owning channel: %d, local role: %d"), *NewObj->GetName(), SpawnMsg->obj().netguid(), SpawnMsg->channelid(), SpawnMsg->localrole());

			// if (SpawnMsg->has_channelid())
			// {
			// 	ChannelDataView->OnSpawnedObject(SpawnedObj, FNetworkGUID(SpawnMsg->obj().netguid()), SpawnMsg->channelid());
			// }

			if (AActor* NewActor = Cast<AActor>(NewObj))
			{
				if (SpawnMsg->has_localrole())
				{
					NewActor->SetRole((ENetRole)SpawnMsg->localrole());
				}

				// UChanneldNetDriver::NotifyActorChannelOpen doesn't always get called in ChanneldUtils::GetObjectByRef.
				NotifyActorChannelOpen(nullptr, NewActor);
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
		UChanneldNetConnection* ClientConnection;
		if (ClientConnectionMap.RemoveAndCopyValue(ResultMsg->connid(), ClientConnection))
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
			// Will also destroy the player controller.
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

ChannelId UChanneldNetDriver::GetSendToChannelId(UChanneldNetConnection* NetConn) const
{
	// Server view can implement the mapping of connId -> channelId.
	if (IsServer())
	{
		if (ChannelDataView.IsValid())
		{
			ChannelId Result;
			if (ChannelDataView->GetSendToChannelId(NetConn, Result))
			{
				return Result;
			}
		}
	}
	
	return LowLevelSendToChannelId.Get();
}

UChanneldGameInstanceSubsystem* UChanneldNetDriver::GetSubsystem() const
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

		// Set the GuidCache in the view so it can map UObject(NetGUID) <-> ChannelId
		if (Subsystem->GetChannelDataView())
		{
			ChannelDataView = Subsystem->GetChannelDataView();
			// ChannelDataView->NetDriver = TSharedPtr<UChanneldNetDriver>(this);
		}
		else
		{
			Subsystem->OnViewInitialized.AddWeakLambda(this, [&](UChannelDataView* InView)
			{
				ChannelDataView = InView;
				// InView->NetDriver = TSharedPtr<UChanneldNetDriver>(this);
			});
		}
	}

	ConnToChanneld = GEngine->GetEngineSubsystem<UChanneldConnection>();

	ConnToChanneld->OnUserSpaceMessageReceived.AddUObject(this, &UChanneldNetDriver::OnUserSpaceMessageReceived);

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
			ClientConnectionMap[ClientConnId]->SendData(MessageType_LOW_LEVEL, DataToSend, DataSize);
		}
		else
		{
			GetServerConnection()->SendData(MessageType_LOW_LEVEL, DataToSend, DataSize);
		}
	}
}

void UChanneldNetDriver::LowLevelDestroy()
{
	ChannelDataView = nullptr;
	
	FGameModeEvents::GameModePostLoginEvent.RemoveAll(this);
	
	ClientConnectionMap.Reset();

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
	ConnToChanneld = nullptr;

	Super::LowLevelDestroy();
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
	if (GetMutableDefault<UChanneldSettings>()->bSkipCustomReplication)
	{
		return;
	}

	/* Moved to UChanneldGameInstanceSubsystem::OnActorSpawned
	UChanneldGameInstanceSubsystem* ChanneldSubsystem = GetSubsystem();
	UChannelDataView* View = ChanneldSubsystem->GetChannelDataView();
	FNetworkGUID NetId = GuidCache->GetNetGUID(Actor);
	ChannelId ChId = LowLevelSendToChannelId.Get();
	if (View)
	{
		View->OnSpawnedObject(Actor, NetId, ChId);
	}
	else
	{
		ChanneldSubsystem->OnViewInitialized.AddWeakLambda(Actor, [Actor, NetId, ChId](UChannelDataView* InView)
		{
			InView->OnSpawnedObject(Actor, NetId, ChId);
		});
	}
	*/
	
	if (!Actor->GetIsReplicated())
	{
		return;
	}

	// Already sent
	if (GuidCache->GetNetGUID(Actor).IsValid())
	{
		return;
	}

	// Send the spawning to the clients
	for (auto& Pair : ClientConnectionMap)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->SendSpawnMessage(Actor, Actor->GetRemoteRole());
		}
	}
}

void UChanneldNetDriver::NotifyActorChannelOpen(UActorChannel* Channel, AActor* Actor)
{
	UE_LOG(LogChanneld, Verbose, TEXT("ActorChannelOpen: %s"), *GetNameSafe(Actor));
	if (auto Comp = Actor->GetComponentByClass(UChanneldReplicationComponent::StaticClass()))
	{
		auto RepComp = Cast<UChanneldReplicationComponent>(Comp);
		RepComp->InitOnce();
	}
}

// Runs only on server
void UChanneldNetDriver::OnClientPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	auto NewPlayerConn = Cast<UChanneldNetConnection>(NewPlayer->GetNetConnection());
	if (NewPlayerConn == nullptr)
	{
		UE_LOG(LogChanneld, Error, TEXT("PlayerController doesn't have the UChanneldNetConnection. Failed to spawn the objects in the clients."));
		return;
	}

	/* Unfortunately, a couple of RPC on the PC is called in GameMode::PostLogin BEFORE invoking this event. So we need to handle the RPC properly.
	// Send the PlayerController to the client (in case any RPC on the PC is called but it doesn't have the current channelId when spawned)
	NewPlayerConn->SendSpawnMessage(NewPlayer, NewPlayer->GetRemoteRole());
	*/

	// Send the GameStateBase to the new player
	auto Comp = Cast<UChanneldReplicationComponent>(NewPlayer->GetComponentByClass(UChanneldReplicationComponent::StaticClass()));
	if (Comp)
	{
		NewPlayerConn->SendSpawnMessage(GameMode->GameState, ENetRole::ROLE_SimulatedProxy);
	}
	else
	{
		UE_LOG(LogChanneld, Warning, TEXT("PlayerController is missing UChanneldReplicationComponent. Failed to spawn the GameStateBase in the client."));
	}

	/* OnServerSpawnedActor() sends the spawning of the new player's pawn to other clients */

	// Send the existing player pawns to the new player
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PC = Iterator->Get();
		if (PC && PC != NewPlayer && PC->GetPawn())
		{
			NewPlayerConn->SendSpawnMessage(PC->GetPawn(), ENetRole::ROLE_SimulatedProxy);
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

// Won't trigger until ClientConnections.Num() > 0
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

			/* Try to reproduce native UE's way of spawning new objects but didn't work
			 * because there are a lot of unrelated objects in the ObjectLookup that failed to be spawned in the client.
			auto PackageMap = CastChecked<UPackageMapClient>(Connection->PackageMap);
			for (auto& Pair : GuidCache->ObjectLookup)
			{
				if (!PackageMap->NetGUIDExportCountMap.Contains(Pair.Key))
				{
					unrealpb::SpawnObjectMessage SpawnMsg;
					SpawnMsg.mutable_obj()->MergeFrom(ChanneldUtils::GetRefOfObject(Pair.Value.Object.Get(), Connection));
					Connection->SendMessage(MessageType_SPAWN, SpawnMsg);
				}
			}
			*/

			if (Connection->PlayerController && Connection->PlayerController->GetViewTarget())
			{
				// Trigger ClientMoveResponse RPC
				Connection->PlayerController->SendClientAdjustment();
			}
		}

		auto Subsystem = GetSubsystem();
		if (Subsystem && ChannelDataView.IsValid())
		{
			Result = ChannelDataView->SendAllChannelUpdates();
		}
	}
	
	return Result;
}

void UChanneldNetDriver::ProcessRemoteFunction(class AActor* Actor, class UFunction* Function, void* Parameters, struct FOutParmRec* OutParms, struct FFrame* Stack, class UObject* SubObject /*= nullptr*/)
{
	const FName FuncFName = Function->GetFName();
	UE_CLOG(FuncFName != ServerMovePackedFuncName && FuncFName != ClientMoveResponsePackedFuncName, LogChanneld, Verbose, TEXT("Sending RPC %s::%s, SubObject: %s"), *Actor->GetName(), *Function->GetName(), *GetNameSafe(SubObject));
	// if (Function->GetFName() == FName("ClientSetHUD"))
	// {
	// 	UE_DEBUG_BREAK();
	// }

	if (!GetMutableDefault<UChanneldSettings>()->bSkipCustomRPC)
	{
		const FString FuncName = FuncFName.ToString();
		auto RepComp = Cast<UChanneldReplicationComponent>(Actor->FindComponentByClass(UChanneldReplicationComponent::StaticClass()));
		if (RepComp)
		{
			UChanneldNetConnection* NetConn = ConnToChanneld->IsClient() ? GetServerConnection() : Cast<UChanneldNetConnection>(Actor->GetNetConnection());
			if (NetConn)
			{
				bool bSuccess = true;
				auto ParamsMsg = RepComp->SerializeFunctionParams(Actor, Function, Parameters, bSuccess);
				if (bSuccess)
				{
					// If the target object hasn't been spawned in the remote end yet, send the Spawn message before the RPC message.
					if (!GuidCache->GetNetGUID(Actor).IsValid())
					{
						NetConn->SendSpawnMessage(Actor, Actor->GetRemoteRole());
					}
				
					unrealpb::RemoteFunctionMessage RpcMsg;
					RpcMsg.mutable_targetobj()->MergeFrom(ChanneldUtils::GetRefOfObject(Actor));
					RpcMsg.set_functionname(TCHAR_TO_UTF8(*FuncName), FuncName.Len());
					if (ParamsMsg)
					{
						RpcMsg.set_paramspayload(ParamsMsg->SerializeAsString());
						UE_LOG(LogChanneld, VeryVerbose, TEXT("Serialized RPC parameters to %dB: %s"), RpcMsg.paramspayload().size(), UTF8_TO_TCHAR(ParamsMsg->DebugString().c_str()));
					}
					uint8* Data = new uint8[RpcMsg.ByteSizeLong()];
					if (RpcMsg.SerializeToArray(Data, RpcMsg.GetCachedSize()))
					{
						NetConn->SendData(MessageType_RPC, Data, RpcMsg.GetCachedSize(), ChannelDataView->GetOwningChannelId(Actor));
						OnSentRPC(Actor, FuncName);
						return;
					}

					UE_LOG(LogChanneld, Warning, TEXT("Failed to serialize RPC proto message: %s::%s"), *Actor->GetName(), *FuncName);
					// TODO: delete Data?
				}
				else
				{
					UE_LOG(LogChanneld, Warning, TEXT("Failed to serialize RPC function params: %s::%s"), *Actor->GetName(), *FuncName);
				}
			}
			else
			{
				UE_LOG(LogChanneld, Warning, TEXT("Failed to send RPC %s::%s as the actor doesn't have any client connection"), *Actor->GetName(), *FuncName);
			}
		}
		else
		{
			UE_LOG(LogChanneld, Warning, TEXT("Can't find the ReplicationComponent to serialize RPC: %s::%s"), *Actor->GetName(), *FuncName);
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
	UE_CLOG(FunctionName != ServerMovePackedFuncName && FunctionName != ClientMoveResponsePackedFuncName, LogChanneld, Verbose, TEXT("Received RPC %s::%s"), *Actor->GetName(), *FunctionName.ToString());

	UFunction* Function = Actor->FindFunction(FunctionName);
	if (!Function)
	{
		UE_LOG(LogChanneld, Error, TEXT("RPC function %s doesn't exist on Actor %s"), *FunctionName.ToString(), *Actor->GetName());
		return;
	}

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

void UChanneldNetDriver::TickFlush(float DeltaSeconds)
{
	// Trigger the callings of ServerReplicateActors() and LowLevelSend()
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

void UChanneldNetDriver::NotifyActorDestroyed(AActor* Actor, bool IsSeamlessTravel)
{
	Super::NotifyActorDestroyed(Actor, IsSeamlessTravel);

	/*
	if (!IsServer())
		return;
	
	FNetworkGUID NetGUID = GuidCache->GetOrAssignNetGUID( Actor );
	if (NetGUID.IsDefault())
	{
		UE_LOG(LogNet, Error, TEXT("CreateDestructionInfo got an invalid NetGUID for %s"), *Actor->GetName());
		return;
	}

	TUniquePtr<FActorDestructionInfo>& NewInfoPtr = DestroyedStartupOrDormantActors.FindOrAdd( NetGUID );
	if (NewInfoPtr.IsValid() == false)
	{
		NewInfoPtr = MakeUnique<FActorDestructionInfo>();
	}

	FActorDestructionInfo &NewInfo = *NewInfoPtr;
	NewInfo.DestroyedPosition = Actor->GetActorLocation();
	NewInfo.NetGUID = NetGUID;
	NewInfo.Level = Actor->GetLevel();
	NewInfo.ObjOuter = Actor->GetOuter();
	NewInfo.PathName = Actor->GetName();

	// Look for renamed actor now so we can clear it after the destroy is queued
	FName RenamedPath = RenamedStartupActors.FindRef(Actor->GetFName());
	if (RenamedPath != NAME_None)
	{
		NewInfo.PathName = RenamedPath.ToString();
	}

	if (NewInfo.Level.IsValid() && !NewInfo.Level->IsPersistentLevel() )
	{
		NewInfo.StreamingLevelName = NewInfo.Level->GetOutermost()->GetFName();
	}
	else
	{
		NewInfo.StreamingLevelName = NAME_None;
	}

	NewInfo.Reason = EChannelCloseReason::Destroyed;

	for( int32 i=ClientConnections.Num()-1; i>=0; i-- )
	{
		UNetConnection* Connection = ClientConnections[i];
		SendDestructionInfo(Connection, NewInfoPtr.Get());
	}
	*/
}
