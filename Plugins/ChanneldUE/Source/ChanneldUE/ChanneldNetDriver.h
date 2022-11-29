// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChanneldTypes.h"
#include "ChanneldConnection.h"
#include "ChannelDataProvider.h"
#include "ChanneldNetConnection.h"
#include "Engine/NetDriver.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "google/protobuf/message.h"
#include "View/ChannelDataView.h"
#include "ChanneldNetDriver.generated.h"

/**
 * 
 */
UCLASS(transient, config=Engine)
class CHANNELDUE_API UChanneldNetDriver : public UNetDriver
{
	GENERATED_BODY()

public:

	// Constructors.
	UChanneldNetDriver(const FObjectInitializer& ObjectInitializer);

	//~ Begin UObject Interface
	virtual void PostInitProperties() override;
	//~ End UObject Interface

	//~ Begin UNetDriver Interface.
	virtual bool IsAvailable() const override;
	// Always use UChanneldNetConnection
	virtual bool InitConnectionClass() override;
	// Connect to channeld
	virtual bool InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error) override;
	// Create NetConnection for client (ServerConnection)
	virtual bool InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error) override;
	// Create message handlers for server
	virtual bool InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error) override;
	// Receive packets from channeld by calling ChanneldConnection.TickIncoming()
	virtual void TickDispatch(float DeltaTime) override;
	// Send packets to channeld by calling ChanneldConnection.TickOutgoing()
	virtual void TickFlush(float DeltaSeconds) override;

	virtual class ISocketSubsystem* GetSocketSubsystem() override;
	virtual FSocket* GetSocket();
	// Client: send raw packet to server via channeld
	// Server: send or broadcast ServerForwardMessage to client via channeld
	virtual void LowLevelSend(TSharedPtr<const FInternetAddr> Address, void* Data, int32 CountBits, FOutPacketTraits& Traits) override;
	virtual void LowLevelDestroy() override;
	virtual bool IsNetResourceValid(void) override;

	virtual void NotifyActorChannelOpen(UActorChannel* Channel, AActor* Actor) override;
	virtual int32 ServerReplicateActors(float DeltaSeconds) override;
	virtual void ProcessRemoteFunction(class AActor* Actor, class UFunction* Function, void* Parameters, struct FOutParmRec* OutParms, struct FFrame* Stack, class UObject* SubObject = nullptr) override;
	virtual void NotifyActorDestroyed(AActor* Actor, bool IsSeamlessTravel) override;
	//~ End UNetDriver Interface
	
	UChanneldNetConnection* AddChanneldClientConnection(ConnectionId ClientConnId);

	void ReceivedRPC(AActor* Actor, const FName& FunctionName, const std::string& ParamsPayload, bool& bDelayRPC);

	UChanneldConnection* GetConnToChanneld() const { return ConnToChanneld; }

	ConnectionId AddrToConnId(const FInternetAddr& Addr);
	TSharedRef<FInternetAddr> ConnIdToAddr(ConnectionId ConnId);

	UChanneldNetConnection* GetServerConnection() const
	{
		return CastChecked<UChanneldNetConnection>(ServerConnection);
	}

	UChanneldNetConnection* GetClientConnection(ConnectionId ConnId) const
	{
		return ClientConnectionMap.FindRef(ConnId);
	}

	virtual ChannelId GetSendToChannelId(UChanneldNetConnection* NetConn) const;

	// Update the PackageMap of all connections that the specified NetId has been sent, so it's safe to send the actor's RPC message.
	void SetAllSentSpawn(const FNetworkGUID NetId);
	
	TWeakObjectPtr<UChannelDataView> ChannelDataView;

protected:
	TSharedRef<ChannelId> LowLevelSendToChannelId = MakeShared<ChannelId>(GlobalChannelId);

private:
	
	const FName ServerMovePackedFuncName = FName("ServerMovePacked");
	const FName ClientMoveResponsePackedFuncName = FName("ClientMoveResponsePacked");
	const FName ServerUpdateCameraFuncName = FName("ServerUpdateCamera");

	// Prevent the engine from GC the connection
	UPROPERTY()
	UChanneldConnection* ConnToChanneld;

	FURL InitBaseURL;

	// Cache the FInternetAddr so it won't be created again every time when mapping from ConnectionId.
	TMap<ConnectionId, TSharedRef<FInternetAddr>> CachedAddr;

	UPROPERTY()
	TMap<uint32, UChanneldNetConnection*> ClientConnectionMap;

	// RPCs queued on the callee's side that dont' have the actor resolved yet.
	TArray<TSharedPtr<unrealpb::RemoteFunctionMessage>> UnprocessedRPCs;

	TSet<FNetworkGUID> SentSpawnedNetGUIDs;

	void OnChanneldAuthenticated(UChanneldConnection* Conn);
	void OnUserSpaceMessageReceived(uint32 MsgType, ChannelId ChId, ConnectionId ClientConnId, const std::string& Payload);
	void OnClientSpawnObject(TSharedRef<unrealpb::SpawnObjectMessage> SpawnMsg);
	void HandleCustomRPC(TSharedPtr<unrealpb::RemoteFunctionMessage> Msg);
	void OnClientPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
	void OnServerSpawnedActor(AActor* Actor);
	void ServerHandleUnsub(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);

	void OnSentRPC(class AActor* Actor, FString FuncName);

	UChanneldGameInstanceSubsystem* GetSubsystem() const;
};
