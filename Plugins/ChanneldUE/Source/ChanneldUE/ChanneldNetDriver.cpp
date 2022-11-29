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

void UChanneldNetDriver::OnClientSpawnObject(TSharedRef<unrealpb::SpawnObjectMessage> SpawnMsg)
{
	FNetworkGUID NetId = FNetworkGUID(SpawnMsg->obj().netguid());
	
	// If the object with the same NetId exists, destroy it before spawning a new one.
	UObject* OldObj = ChanneldUtils::GetObjectByRef(&SpawnMsg->obj(), GetWorld(), false);
	if (OldObj)
	{
		UE_LOG(LogChanneld, Log, TEXT("[Client] Found spawned object %s of duplicated NetId: %d, will be destroyed."), *GetNameSafe(OldObj), SpawnMsg->obj().netguid());

		GuidCache->ObjectLookup.Remove(NetId);
		GuidCache->NetGUIDLookup.Remove(OldObj);
			
		if (AActor* OldActor = Cast<AActor>(OldObj))
		{
			GetWorld()->DestroyActor(OldActor);
		}
	}

	if (ChannelDataView.IsValid() && SpawnMsg->has_channelid())
	{
		// Set up the mapping before actually spawn it, so AddProvider() can find the mapping.
		ChannelDataView->SetOwningChannelId(NetId, SpawnMsg->channelid());
			
		// Also add the mapping of all context NetGUIDs
		for (auto& ContextObj : SpawnMsg->obj().context())
		{
			ChannelDataView->SetOwningChannelId(ContextObj.netguid(), SpawnMsg->channelid());
		}
	}
		
	UObject* NewObj = ChanneldUtils::GetObjectByRef(&SpawnMsg->obj(), GetWorld());
	if (NewObj)
	{
		// if (SpawnMsg->has_channelid())
		// {
		// 	ChannelDataView->OnSpawnedObject(SpawnedObj, FNetworkGUID(SpawnMsg->obj().netguid()), SpawnMsg->channelid());
		// }

		ENetRole LocalRole = static_cast<ENetRole>(SpawnMsg->localrole());
		if (AActor* NewActor = Cast<AActor>(NewObj))
		{
				/*
			// The first PlayerController on client side is AutonomousProxy; others are SimulatedProxy.
			if (NewActor->IsA<APlayerController>())
			{
				LocalRole = NewActor == GetWorld()->GetFirstPlayerController() ? ENetRole::ROLE_AutonomousProxy : ENetRole::ROLE_SimulatedProxy;
				// TEST: don't spawn other PC!
				if (LocalRole == ENetRole::ROLE_SimulatedProxy)
				{
					GetWorld()->DestroyActor(NewActor);
					return;
				}
				NewActor->SetRole(LocalRole);
			}
			else*/ if (SpawnMsg->has_localrole())
			{
				NewActor->SetRole(LocalRole);
			}

			if (SpawnMsg->has_owningconnid())
			{
				ChanneldUtils::SetActorRoleByOwningConnId(NewActor, SpawnMsg->owningconnid());
				LocalRole = NewActor->GetLocalRole();
			}

			// UChanneldNetDriver::NotifyActorChannelOpen doesn't always get called in ChanneldUtils::GetObjectByRef.
			NotifyActorChannelOpen(nullptr, NewActor);
		}

		if (ChannelDataView.IsValid())
		{
			ChannelDataView->OnClientSpawnedObject(NewObj, SpawnMsg->channelid());
		}

		UE_LOG(LogChanneld, Verbose, TEXT("[Client] Spawned object from message: %s, NetId: %d, owning channel: %d, local role: %d"), *NewObj->GetName(), SpawnMsg->obj().netguid(), SpawnMsg->channelid(), LocalRole);
	}
	else
	{
		UE_LOG(LogChanneld, Warning, TEXT("[Client] Failed to spawn object from msg: %s"), UTF8_TO_TCHAR(SpawnMsg->DebugString().c_str()));
	}
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

		OnClientSpawnObject(SpawnMsg);
	}
	else if (MsgType == MessageType_DESTROY)
	{
		auto DestroyMsg = MakeShared<unrealpb::DestroyObjectMessage>();
		if (!DestroyMsg->ParseFromString(Payload))
		{
			UE_LOG(LogChanneld, Error, TEXT("Failed to parse DestroyObjectMessage"));
			return;
		}

		UObject* ObjToDestroy = GuidCache->GetObjectFromNetGUID(FNetworkGUID(DestroyMsg->netid()), true);
		if (ObjToDestroy)
		{
			if (ObjToDestroy->IsA<AActor>())
			{
				GetWorld()->DestroyActor(Cast<AActor>(ObjToDestroy));
			}
			else
			{
				ObjToDestroy->ConditionalBeginDestroy();
			}
		}
		else
		{
			UE_LOG(LogChanneld, Warning, TEXT("[Client] Failed to destroy object from msg: %s"), UTF8_TO_TCHAR(DestroyMsg->DebugString().c_str()));
		}
	}
}

void UChanneldNetDriver::HandleCustomRPC(TSharedPtr<unrealpb::RemoteFunctionMessage> Msg)
{
	// FIXME: using IsServer() is not precise here. Should have ownerConnId in the message and use it to compare with local connId to see if local has authority over the actor.
	bool bHasAuthority = IsServer();
	AActor* Actor = Cast<AActor>(ChanneldUtils::GetObjectByRef(&Msg->targetobj(), GetWorld(), !bHasAuthority));
	if (!Actor)
	{
		UE_LOG(LogChanneld, Warning, TEXT("Cannot find actor to call remote function '%s', NetGUID: %d. %s"), UTF8_TO_TCHAR(Msg->functionname().c_str()), Msg->targetobj().netguid(), bHasAuthority ? TEXT("The message will be dropped.") : TEXT("Pushed to the next tick."));
		if (!bHasAuthority)
		{
			UnprocessedRPCs.Add(Msg);
		}
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

	/* Newly spawned actor always has LocalRole = Authority
	if (!Actor->HasAuthority())
	{
		return;
	}
	*/

	// Send the spawn of PC in OnClientPostLogin instead, because we only want to send PC to owning client,
	// but at this moment, Actor doesn't have NetConnection set yet.
	if (Actor->IsA<APlayerController>())
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
	if (ChannelDataView.IsValid())
	{
		FNetworkGUID NetId = GuidCache->GetOrAssignNetGUID(Actor);
		if (!ChannelDataView->OnServerSpawnedObject(Actor, NetId))
		{
			return;
		}
	}
	else
	{
		UE_LOG(LogChanneld, Warning, TEXT("ChannelDataView is not initialized yet. If the actor '%s' is a DataProvider, it will not be registered."), *Actor->GetName());
	}

	if (!Actor->GetIsReplicated())
	{
		return;
	}

	/* At this moment, PlayerController doesn't have RemoteRole or NetConnection set yet.
	 * We should wait and send the spawn message later, but a couple of RPC on the PC will be called in GameMode::PostLogin immediately.
	if (Actor->IsA<APlayerController>())
	{
		return;
	}
	*/

	/*
	if (Actor->GetComponentsByInterface(UChannelDataProvider::StaticClass()).Num() == 0)
	{
		UE_LOG(LogChanneld, Warning, TEXT("[Server] Replicating actor %s doesn't implement IChannelDataProvider, will not be spawn to client."), *Actor->GetName());
		return;
	}
	*/

	/*
	// Already sent
	const FNetworkGUID NetId = GuidCache->GetNetGUID(Actor);
	if (NetId.IsValid() && SentSpawnedNetGUIDs.Contains(NetId))
	{
		return;
	}
	*/

	// Send the spawning to the clients
	for (auto& Pair : ClientConnectionMap)
	{
		if (IsValid(Pair.Value))
		{
			uint32 OwningConnId = 0;
			if (auto NetConn = Cast<UChanneldNetConnection>(Actor->GetNetConnection()))
			{
				OwningConnId = NetConn->GetConnId();
			}

			if (ChannelDataView.IsValid())
			{
				ChannelDataView->SendSpawnToConn(Actor, Pair.Value, OwningConnId);
			}
			else
			{
				Pair.Value->SendSpawnMessage(Actor, Actor->GetRemoteRole(), OwningConnId);
			}
		}
	}

	// TODO: Send the spawning to nearby spatial servers that sub to the actor's spatial channel (if exist)
	
}

void UChanneldNetDriver::SetAllSentSpawn(const FNetworkGUID NetId)
{
	for (auto& Pair : ClientConnectionMap)
	{
		Pair.Value->SetSentSpawned(NetId);
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
	
	ChannelDataView->OnClientPostLogin(GameMode, NewPlayer, NewPlayerConn);
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
	UE_CLOG(FuncFName != ServerMovePackedFuncName && FuncFName != ClientMoveResponsePackedFuncName && FuncFName != ServerUpdateCameraFuncName, LogChanneld, Verbose,
		TEXT("Sending RPC %s::%s, SubObject: %s"), *Actor->GetName(), *Function->GetName(), *GetNameSafe(SubObject));
	// if (Function->GetFName() == FName("ClientSetHUD"))
	// {
	// 	UE_DEBUG_BREAK();
	// }
	
	if (Actor->IsActorBeingDestroyed())
	{
		UE_LOG(LogNet, Warning, TEXT("UNetDriver::ProcessRemoteFunction: Remote function %s called from actor %s while actor is being destroyed. Function will not be processed."), *Function->GetName(), *Actor->GetName());
		return;
	}
	
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
					NetConn->SendRPCMessage(Actor, FuncName, ParamsMsg, ChannelDataView->GetOwningChannelId(Actor));
					OnSentRPC(Actor, FuncName);
					return;
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
	UE_CLOG(FunctionName != ServerMovePackedFuncName && FunctionName != ClientMoveResponsePackedFuncName && FunctionName != ServerUpdateCameraFuncName,
		LogChanneld, Verbose, TEXT("Received RPC %s::%s"), *Actor->GetName(), *FunctionName.ToString());

	if (Actor->GetLocalRole() <= ENetRole::ROLE_SimulatedProxy)
	{
		bDelayRPC = true;
		// TODO: if spatial server, forward the message to the server that has authority over the actor.
		UE_LOG(LogChanneld, Warning, TEXT("Local role has no authroity to process RPC %s::%s"), *Actor->GetName(), *FunctionName.ToString());
		return;
	}
	
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
	// IMPORTANT: offset with the ConnId to avoid NetworkGUID conflicts
	const uint32 UniqueNetIdOffset = ConnToChanneld->GetConnId() << ConnectionIdBitOffset;
	GuidCache->UniqueNetIDs[0] = UniqueNetIdOffset;
	GuidCache->UniqueNetIDs[1] = UniqueNetIdOffset;

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
	if (GetMutableDefault<UChanneldSettings>()->bSkipCustomReplication)
	{
		Super::NotifyActorDestroyed(Actor, IsSeamlessTravel);
		return;
	}

	if (!Actor->HasAuthority())
	{
		return;
	}

	FNetworkGUID NetId = GuidCache->GetNetGUID(Actor);
	if (NetId.IsValid())
	{
		return;
	}
	
	for (auto& Pair : ClientConnectionMap)
	{
		Pair.Value->SendDestroyMessage(Actor);
		Pair.Value->NotifyActorDestroyed(Actor);
	}

	// TODO: Use BroadcastType.ADJACENT_CHANNELS instead of sending to each connection.

	if (ServerConnection)
	{
		ServerConnection->NotifyActorDestroyed(Actor);
	}

	RemoveNetworkActor(Actor);

	if (ChannelDataView.IsValid())
	{
		ChannelDataView->OnDestroyedObject(Actor, NetId);
	}
}
