#include "ChanneldNetConnection.h"
#include "ChanneldNetDriver.h"
#include "Net/DataChannel.h"
#include "PacketHandler.h"
#include "PacketHandlers/StatelessConnectHandlerComponent.h"

UChanneldNetConnection::UChanneldNetConnection(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
}

void UChanneldNetConnection::InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket /*= 0*/, int32 InPacketOverhead /*= 0*/)
{
	// Pass the call up the chain
	Super::InitBase(InDriver, InSocket, InURL, InState,
		// Use the default packet size/overhead unless overridden by a child class
		InMaxPacket == 0 ? MaxPacketSize : InMaxPacket,
		InPacketOverhead == 0 ? MinPacketSize : InPacketOverhead);

	if (bDisableHandshaking)
	{
		DisableAddressResolution();
		// Reset the PacketHandler to remove the StatelessConnectHandler and bypass the handshake process.
		Handler.Reset(NULL);
	}
}

void UChanneldNetConnection::InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket /*= 0*/, int32 InPacketOverhead /*= 0*/)
{
	InitBase(InDriver, InSocket, InURL, InState,
		// Use the default packet size/overhead unless overridden by a child class
		InMaxPacket == 0 ? MaxPacketSize : InMaxPacket,
		InPacketOverhead == 0 ? MinPacketSize : InPacketOverhead);

	InitSendBuffer();

}

void UChanneldNetConnection::InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket /*= 0*/, int32 InPacketOverhead /*= 0*/)
{
	InitBase(InDriver, InSocket, InURL, InState,
		// Use the default packet size/overhead unless overridden by a child class
		InMaxPacket == 0 ? MaxPacketSize : InMaxPacket,
		InPacketOverhead == 0 ? MinPacketSize : InPacketOverhead);

	InitSendBuffer();

	// This is for a client that needs to log in, setup ClientLoginState and ExpectedClientLoginMsgType to reflect that
	SetClientLoginState(EClientLoginState::LoggingIn);
	SetExpectedClientLoginMsgType(NMT_Hello);
}

void UChanneldNetConnection::LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits)
{
	//Super::LowLevelSend(Data, CountBits, Traits);

	auto ChanneldDriver = CastChecked<UChanneldNetDriver>(Driver);
	ChanneldDriver->LowLevelSend(RemoteAddr, Data, CountBits, Traits);
}

void UChanneldNetConnection::ReceivedRawPacket(void* Data, int32 Count)
{
	if (Count == 0 ||   // nothing to process
		Driver == NULL) // connection closing
	{
		return;
	}

	uint8* DataRef = reinterpret_cast<uint8*>(Data);
	if (bInConnectionlessHandshake)
	{
		// Process all incoming packets.
		if (Driver->ConnectionlessHandler.IsValid() && Driver->StatelessConnectComponent.IsValid() && Driver->GetSocketSubsystem() != nullptr)
		{
			TSharedPtr<FInternetAddr> IncomingAddress = RemoteAddr->Clone();
			//UE_LOG(LogNet, Log, TEXT("%s received raw packet from: %s"), Driver->IsServer() ? "Server" : "Client", *(IncomingAddress->ToString(true)));

			const ProcessedPacket UnProcessedPacket =
				Driver->ConnectionlessHandler->IncomingConnectionless(IncomingAddress, DataRef, Count);

			TSharedPtr<StatelessConnectHandlerComponent> StatelessConnect = Driver->StatelessConnectComponent.Pin();

			bool bRestartedHandshake = false;

			if (!UnProcessedPacket.bError && StatelessConnect->HasPassedChallenge(IncomingAddress, bRestartedHandshake) &&
				!bRestartedHandshake)
			{
				UE_LOG(LogNet, Log, TEXT("Server accepting post-challenge connection from: %s"), *(IncomingAddress->ToString(false)));
				// Set the initial packet sequence from the handshake data
				if (StatelessConnectComponent.IsValid())
				{
					int32 ServerSequence = 0;
					int32 ClientSequence = 0;
					StatelessConnect->GetChallengeSequence(ServerSequence, ClientSequence);
					InitSequence(ClientSequence, ServerSequence);
				}

				if (Handler.IsValid())
				{
					Handler->BeginHandshaking();
				}

				bInConnectionlessHandshake = false; // i.e. bPassedChallenge
				UE_LOG(LogNet, Warning, TEXT("UWebSocketConnection::bChallengeHandshake: %s"), *LowLevelDescribe());
				Count = FMath::DivideAndRoundUp(UnProcessedPacket.CountBits, 8);
				if (Count > 0)
				{
					DataRef = UnProcessedPacket.Data;
				}
				else
				{
					return; // NO FURTHER DATA TO PROCESS
				}
			}
			else
			{
				// WARNING: if here, it might be during (bInitialConnect) - which needs to be processed (ReceivedRawPacket)
				//return;
			}
		}
	}

	UNetConnection::ReceivedRawPacket(DataRef, Count);
}

void UChanneldNetConnection::ServerReceivedRawPacket(void* Data, int32 Count)
{
	if (!bDisableHandshaking && bInConnectionlessHandshake)
	{
		const ProcessedPacket RawPacket =
			this->Driver->ConnectionlessHandler->IncomingConnectionless(RemoteAddr, (uint8*)Data, Count);
		auto StatelessConnect = this->Driver->StatelessConnectComponent.Pin();
		bool bRestartedHandshake = false;
		if (!RawPacket.bError && StatelessConnect->HasPassedChallenge(RemoteAddr, bRestartedHandshake) &&
			!bRestartedHandshake)
		{
			if (this->StatelessConnectComponent.IsValid())
			{
				int32 ServerSequence = 0;
				int32 ClientSequence = 0;
				StatelessConnect->GetChallengeSequence(ServerSequence, ClientSequence);
				this->InitSequence(ClientSequence, ServerSequence);
			}

			if (this->Handler.IsValid())
			{
				this->Handler->BeginHandshaking();
			}

			this->bInConnectionlessHandshake = false;

			// Reset the challenge data for the future
			if (StatelessConnect.IsValid())
			{
				StatelessConnect->ResetChallengeData();
			}

			int32 RawPacketSize = FMath::DivideAndRoundUp(RawPacket.CountBits, 8);
			if (RawPacketSize == 0)
			{
				// No actual data to receive.
				return;
			}

			// Forward the data from the processed packet.
			ReceivedRawPacket(RawPacket.Data, RawPacketSize);
			return;
		}

	}

	ReceivedRawPacket(Data, Count);
}

void UChanneldNetConnection::ClientReceivedRawPacket(void* Data, int32 Count)
{
	if (!bDisableHandshaking && bInConnectionlessHandshake)
	{
		const ProcessedPacket RawPacket = Handler->Incoming((uint8*)Data, Count);
		int32 RawPacketSize = FMath::DivideAndRoundUp(RawPacket.CountBits, 8);
		if (RawPacketSize == 0)
		{
			// No actual data to receive.
			return;
		}

		// Forward the data from the processed packet.
		ReceivedRawPacket(RawPacket.Data, RawPacketSize);
		return;
	}

	ReceivedRawPacket(Data, Count);
}

