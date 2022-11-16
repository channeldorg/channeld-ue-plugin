#pragma once

#include "CoreMinimal.h"
#include "Engine/NetConnection.h"
#include "ChanneldTypes.h"
#include "unreal_common.pb.h"
#include "ChanneldNetConnection.generated.h"

UCLASS(transient, config=ChanneldUE)
class CHANNELDUE_API UChanneldNetConnection : public UNetConnection
{
	GENERATED_BODY()

public:

	// Constructors.
	UChanneldNetConnection(const FObjectInitializer& ObjectInitializer);

	virtual void InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits) override;
	virtual FString LowLevelGetRemoteAddress(bool bAppendPort = false) override;
	virtual FString LowLevelDescribe() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void ReceivedRawPacket(void* Data, int32 Count) override;

	FORCEINLINE uint32 GetConnId()
	{
		uint32 ConnId;
		RemoteAddr->GetIp(ConnId);
		return ConnId;
	}

	// Send data between UE client and sever via channeld. MsgType should be in user space (>= 100).
	void SendData(uint32 MsgType, const uint8* DataToSend, int32 DataSize, ChannelId ChId = InvalidChannelId);
	// Send message between UE client and sever via channeld. MsgType should be in user space (>= 100).
	void SendMessage(uint32 MsgType, const google::protobuf::Message& Msg, ChannelId ChId = InvalidChannelId);
	bool HasSentSpawn(UObject* Object) const;
	void SendSpawnMessage(UObject* Object, ENetRole Role = ENetRole::ROLE_None);
	// Flush the handshake packets that are queued before received AuthResultMessage to the server.
	void FlushUnauthData();

	bool bDisableHandshaking = false;
	bool bInConnectionlessHandshake = false;
	bool bChanneldAuthenticated = false;

	//ChannelId LowLevelSendToChannelId = GlobalChannelId;

private:

	//uint32 ConnId = 0;
	
	// Queue the data from LowLevelSend() when the connection and authentication to channeld is not finished yet,
	// and send them after the authentication is done.
	TQueue<TTuple<std::string*, FOutPacketTraits*>> LowLevelSendDataBeforeAuth;
};