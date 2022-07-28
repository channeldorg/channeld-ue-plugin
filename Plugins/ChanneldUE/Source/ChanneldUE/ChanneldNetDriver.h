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
#include "Test.pb.h"
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
	virtual bool InitConnectionClass() override;
	virtual bool InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error) override;
	virtual bool InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error) override;
	virtual bool InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error) override;
	virtual void TickDispatch(float DeltaTime) override;
	virtual void TickFlush(float DeltaSeconds) override;

	virtual class ISocketSubsystem* GetSocketSubsystem() override;
	virtual FUniqueSocket CreateSocketForProtocol(const FName& ProtocolType) override;
	virtual FUniqueSocket CreateAndBindSocket(TSharedRef<FInternetAddr> BindAddr, int32 Port, bool bReuseAddressAndPort, int32 DesiredRecvSize, int32 DesiredSendSize, FString& Error) override;
	virtual FSocket* GetSocket() override;
	virtual void LowLevelSend(TSharedPtr<const FInternetAddr> Address, void* Data, int32 CountBits, FOutPacketTraits& Traits) override;
	virtual void LowLevelDestroy() override;
	virtual bool IsNetResourceValid(void) override;
	//~ End UNetDriver Interface

	virtual int32 ServerReplicateActors(float DeltaSeconds) override;

	void RegisterChannelDataProvider(IChannelDataProvider* Provider);

	UChanneldConnection* GetConnection() { return ConnToChanneld; }

	ConnectionId AddrToConnId(const FInternetAddr& Addr);
	FInternetAddr& ConnIdToAddr(ConnectionId ConnId);

	UPROPERTY(Config)
	FString ChanneldIpForClient = "127.0.0.1";
	UPROPERTY(Config)
	uint32 ChanneldPortForClient = 12108;
	UPROPERTY(Config)
	FString ChanneldIpForServer = "127.0.0.1";
	UPROPERTY(Config)
	uint32 ChanneldPortForServer = 11288;

	//TMap<ChannelId, TSubclassOf<google::protobuf::Message>> ChannelDataClasses;

	TSet<IChannelDataProvider*> ChannelDataProviders;

	testpb::TestChannelDataMessage* TestChannelData;

	ChannelId LowLevelSendToChannelId = GlobalChannelId;

private:

	// Prevent the engine from GC the connection
	UPROPERTY()
	UChanneldConnection* ConnToChanneld;

	FURL InitBaseURL;
	TQueue<TTuple<TSharedPtr<const FInternetAddr>, std::string*, FOutPacketTraits*>> LowLevelSendDataBeforeAuth;
	TMap<ConnectionId, TSharedRef<FInternetAddr>> CachedAddr;

	TMap<ConnectionId, UChanneldNetConnection*> ClientConnectionMap;

	void OnChanneldAuthenticated(UChanneldConnection* Conn);
	void HandleChannelDataUpdate(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);

};
