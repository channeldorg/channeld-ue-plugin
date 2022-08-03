// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChanneldTypes.h"
#include "ChanneldConnection.h"
#include "ChannelDataProvider.h"
#include "ChanneldNetConnection.h"
#include "Engine/NetDriver.h"
#include "IpNetDriver.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "google/protobuf/message.h"
#include "ChanneldNetDriver.generated.h"

/**
 * 
 */
UCLASS(transient, config=ChanneldUE)
class CHANNELDUE_API UChanneldNetDriver : public UIpNetDriver
{
	GENERATED_BODY()

public:

	// Constructors.
	UChanneldNetDriver(const FObjectInitializer& ObjectInitializer);

	//~ Begin UObject Interface
	virtual void PostInitProperties() override;
	//~ End UObject Interface

	//~ Begin UNetDriver Interface.
	virtual void Shutdown() override;
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
	virtual FUniqueSocket CreateSocketForProtocol(const FName& ProtocolType) override;
	virtual FUniqueSocket CreateAndBindSocket(TSharedRef<FInternetAddr> BindAddr, int32 Port, bool bReuseAddressAndPort, int32 DesiredRecvSize, int32 DesiredSendSize, FString& Error) override;
	virtual FSocket* GetSocket() override;
	// Client: send raw packet to server via channeld
	// Server: send or broadcast ServerForwardMessage to client via channeld
	virtual void LowLevelSend(TSharedPtr<const FInternetAddr> Address, void* Data, int32 CountBits, FOutPacketTraits& Traits) override;
	virtual void LowLevelDestroy() override;
	virtual bool IsNetResourceValid(void) override;
	//~ End UNetDriver Interface

	virtual int32 ServerReplicateActors(float DeltaSeconds) override;

	void RegisterChannelDataProvider(IChannelDataProvider* Provider);

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


	//TMap<ChannelId, TSubclassOf<google::protobuf::Message>> ChannelDataClasses;

	TSet<IChannelDataProvider*> ChannelDataProviders;

	ChannelId LowLevelSendToChannelId = GlobalChannelId;

private:

	// Prevent the engine from GC the connection
	UPROPERTY()
	UChanneldConnection* ConnToChanneld;

	FURL InitBaseURL;

	// Cache the FInternetAddr so it won't be created again every time when mapping from ConnectionId.
	TMap<ConnectionId, TSharedRef<FInternetAddr>> CachedAddr;

	TMap<ConnectionId, UChanneldNetConnection*> ClientConnectionMap;

	void OnChanneldAuthenticated(UChanneldConnection* Conn);
	UChanneldNetConnection* OnClientConnected(ConnectionId ClientConnId);
	void HandleChannelDataUpdate(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);

};
