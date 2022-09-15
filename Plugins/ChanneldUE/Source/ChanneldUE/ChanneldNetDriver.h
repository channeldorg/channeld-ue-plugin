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
UCLASS(transient, config=ChanneldUE)
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
	// Connect to channeld. Do nothing else.
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
	//~ End UNetDriver Interface

	virtual int32 ServerReplicateActors(float DeltaSeconds) override;

	virtual void ProcessRemoteFunction(class AActor* Actor, class UFunction* Function, void* Parameters, struct FOutParmRec* OutParms, struct FFrame* Stack, class UObject* SubObject = nullptr) override;
	void ReceivedRPC(AActor* Actor, const FName& FunctionName, const std::string& ParamsPayload, TSet<FNetworkGUID>& UnmappedGuids);

	UChanneldConnection* GetConnToChanneld() { return ConnToChanneld; }

	ConnectionId AddrToConnId(const FInternetAddr& Addr);
	TSharedRef<FInternetAddr> ConnIdToAddr(ConnectionId ConnId);

	UChanneldNetConnection* GetServerConnection()
	{
		return CastChecked<UChanneldNetConnection>(ServerConnection);
	}

	UPROPERTY(Config)
	FString ChanneldIpForClient = "127.0.0.1";
	UPROPERTY(Config)
	uint32 ChanneldPortForClient = 12108;
	UPROPERTY(Config)
	FString ChanneldIpForServer = "127.0.0.1";
	UPROPERTY(Config)
	uint32 ChanneldPortForServer = 11288;

	UPROPERTY(Config)
	bool bDisableHandshaking = true;
	UPROPERTY(Config)
	bool bSkipCustomReplication = false;
	UPROPERTY(Config)
	bool bSkipCustomRPC = true;

	TSharedRef<ChannelId> LowLevelSendToChannelId = MakeShared<ChannelId>(GlobalChannelId);

private:

	// Prevent the engine from GC the connection
	UPROPERTY()
	UChanneldConnection* ConnToChanneld;

	FURL InitBaseURL;

	// Cache the FInternetAddr so it won't be created again every time when mapping from ConnectionId.
	TMap<ConnectionId, TSharedRef<FInternetAddr>> CachedAddr;

	TMap<ConnectionId, UChanneldNetConnection*> ClientConnectionMap;

	UChanneldNetConnection* OnClientConnected(ConnectionId ClientConnId);
	void OnChanneldAuthenticated(UChanneldConnection* Conn);
	void OnUserSpaceMessageReceived(uint32 MsgType, ChannelId ChId, ConnectionId ClientConnId, const std::string& Payload);
	void HandleLowLevelMessage(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleRemoteFunctionMessage(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void ServerHandleUnsub(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);

	void SendDataToClient(uint32 MsgType, ConnectionId ClientConnId, uint8* DataToSend, int32 DataSize);
	void SendDataToServer(uint32 MsgType, uint8* DataToSend, int32 DataSize);

	UChanneldGameInstanceSubsystem* GetSubsystem();
};
