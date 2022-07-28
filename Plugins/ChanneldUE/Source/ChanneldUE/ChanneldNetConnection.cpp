#include "ChanneldNetConnection.h"
#include "ChanneldNetDriver.h"

UChanneldNetConnection::UChanneldNetConnection(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	DisableAddressResolution();
}

void UChanneldNetConnection::InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket /*= 0*/, int32 InPacketOverhead /*= 0*/)
{
	Super::InitBase(InDriver, InSocket, InURL, EConnectionState::USOCK_Open, InMaxPacket, InPacketOverhead);
	// Reset the PacketHandler to remove the StatelessConnectHandler and bypass the handshake.
	Handler.Reset(NULL);
}

void UChanneldNetConnection::LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits)
{
	//Super::LowLevelSend(Data, CountBits, Traits);

	auto ChanneldDriver = CastChecked<UChanneldNetDriver>(Driver);
	ChanneldDriver->LowLevelSend(RemoteAddr, Data, CountBits, Traits);
}

