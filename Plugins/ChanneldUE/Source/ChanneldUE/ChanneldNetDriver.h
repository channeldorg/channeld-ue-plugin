// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChanneldTypes.h"
#include "ChanneldConnection.h"
#include "Engine/NetDriver.h"
#include "IpNetDriver.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "google/protobuf/message.h"
#include "Channeld.pb.h"
#include "Test.pb.h"
#include "ChanneldNetDriver.generated.h"

class IChannelDataProvider
{
public:
	virtual channeldpb::ChannelType GetChannelType() = 0;
	virtual ChannelId GetChannelId() = 0;
	virtual void SetChannelId(ChannelId ChId) = 0;
	virtual bool UpdateChannelData(google::protobuf::Message* ChannelData) = 0;
	virtual void OnChannelDataUpdated(const channeldpb::ChannelDataUpdateMessage* UpdateMsg) = 0;
	virtual ~IChannelDataProvider() {}
};

/**
 * 
 */
UCLASS(transient, config=Engine)
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

	UPROPERTY(Config)
	FString ChanneldIpForClient;
	UPROPERTY(Config)
	uint32 ChanneldPortForClient;
	UPROPERTY(Config)
	FString ChanneldIpForServer;
	UPROPERTY(Config)
	uint32 ChanneldPortForServer;

	//TMap<ChannelId, TSubclassOf<google::protobuf::Message>> ChannelDataClasses;

	TSet<IChannelDataProvider*> ChannelDataProviders;

	testpb::TestChannelDataMessage* TestChannelData;

private:
	UChanneldConnection* Connection;

	void HandleAuthResult(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleCreateChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleSubToChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleChannelDataUpdate(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
};
