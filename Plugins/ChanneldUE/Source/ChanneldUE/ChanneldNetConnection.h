#pragma once

#include "CoreMinimal.h"
#include "ChanneldTypes.h"
#include "IpConnection.h"
#include "ChanneldNetConnection.generated.h"

UCLASS(transient, config=ChanneldUE)
class CHANNELDUE_API UChanneldNetConnection : public UIpConnection
{
	GENERATED_BODY()

public:

	// Constructors.
	UChanneldNetConnection(const FObjectInitializer& ObjectInitializer);

	FORCEINLINE uint32 GetConnId() 
	{
		uint32 ConnId;
		RemoteAddr->GetIp(ConnId);
		return ConnId;
	}

	virtual void InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits) override;
	virtual void ReceivedRawPacket(void* Data, int32 Count) override;
	void ServerReceivedRawPacket(void* Data, int32 Count);
	void ClientReceivedRawPacket(void* Data, int32 Count);

	bool bDisableHandshaking = false;
	bool bInConnectionlessHandshake = false;

private:

	//uint32 ConnId = 0;
};