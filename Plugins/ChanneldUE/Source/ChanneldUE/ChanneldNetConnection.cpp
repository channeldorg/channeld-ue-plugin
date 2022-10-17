#include "ChanneldNetConnection.h"
#include "ChanneldNetDriver.h"
#include "ChanneldTypes.h"
#include "Net/DataChannel.h"
#include "PacketHandler.h"
#include "PacketHandlers/StatelessConnectHandlerComponent.h"
#include "Replication/ChanneldReplicationComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "unreal_common.pb.h"
#include "ChanneldUtils.h"

UChanneldNetConnection::UChanneldNetConnection(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	//MaxPacket = MaxPacketSize;
	//SetInternalAck(true);
	//SetAutoFlush(true);
}

void UChanneldNetConnection::InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket /*= 0*/, int32 InPacketOverhead /*= 0*/)
{
	// Pass the call up the chain
	Super::InitBase(InDriver, InSocket, InURL, InState,
		// Use the default packet size/overhead unless overridden by a child class
		InMaxPacket == 0 ? MAX_PACKET_SIZE : InMaxPacket,
		InPacketOverhead == 0 ? MinPacketSize : InPacketOverhead);

	if (bDisableHandshaking)
	{
		//DisableAddressResolution();
		// Reset the PacketHandler to remove the StatelessConnectHandler and bypass the handshake process.
		Handler.Reset(NULL);
	}
}

void UChanneldNetConnection::InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket /*= 0*/, int32 InPacketOverhead /*= 0*/)
{
	InitBase(InDriver, InSocket, InURL, InState, InMaxPacket, InPacketOverhead);

	MaxPacket = MaxPacketSize;
	PacketOverhead = 5;
	InitSendBuffer();
}

void UChanneldNetConnection::InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket /*= 0*/, int32 InPacketOverhead /*= 0*/)
{
	InitBase(InDriver, InSocket, InURL, InState, InMaxPacket, InPacketOverhead);

	RemoteAddr = InDriver->GetSocketSubsystem()->CreateInternetAddr();
	uint32 Ip;
	int32 Port;
	InRemoteAddr.GetIp(Ip);
	InRemoteAddr.GetPort(Port);
	RemoteAddr->SetIp(Ip);
	RemoteAddr->SetPort(Port);

	MaxPacket = MaxPacketSize;
	PacketOverhead = 10;
	InitSendBuffer();

	// This is for a client that needs to log in, setup ClientLoginState and ExpectedClientLoginMsgType to reflect that
	SetClientLoginState(EClientLoginState::LoggingIn);
	SetExpectedClientLoginMsgType(NMT_Hello);
}

void UChanneldNetConnection::LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits)
{
	//Super::LowLevelSend(Data, CountBits, Traits);

	int32 DataSize = FMath::DivideAndRoundUp(CountBits, 8);
	// The packet sent to channeld before the authentication is finished (e.g. Handshake, Join) should be queued
	if (!bChanneldAuthenticated)
	{
		LowLevelSendDataBeforeAuth.Enqueue(MakeTuple(new std::string(reinterpret_cast<const char*>(Data), DataSize), &Traits));
		UE_LOG(LogChanneld, Log, TEXT("NetConnection queued unauthenticated LowLevelSendData, size: %dB"), DataSize);
	}
	else
	{
		const uint8* DataToSend = reinterpret_cast<uint8*>(Data);
		if (!bDisableHandshaking && Handler.IsValid() && !Handler->GetRawSend())
		{
			const ProcessedPacket ProcessedData = Handler->Outgoing(reinterpret_cast<uint8*>(Data), CountBits, Traits);

			if (!ProcessedData.bError)
			{
				DataToSend = ProcessedData.Data;
				CountBits = ProcessedData.CountBits;
				DataSize = FMath::DivideAndRoundUp(CountBits, 8);
			}
			else
			{
				return;
			}
		}

		bool bBlockSend = false;
		LowLevelSendDel.ExecuteIfBound((void*)DataToSend, DataSize, bBlockSend);

		if (!bBlockSend && DataSize > 0)
		{
			auto NetDriver = CastChecked<UChanneldNetDriver>(Driver);
			auto ConnToChanneld = NetDriver->GetConnToChanneld();
			if (ConnToChanneld->IsServer())
			{
				channeldpb::ServerForwardMessage ServerForwardMessage;
				ServerForwardMessage.set_clientconnid(GetConnId());
				ServerForwardMessage.set_payload(DataToSend, DataSize);
				ConnToChanneld->Send(NetDriver->LowLevelSendToChannelId.Get(), MessageType_LOW_LEVEL, ServerForwardMessage, channeldpb::SINGLE_CONNECTION);
			}
			else
			{
				ConnToChanneld->SendRaw(NetDriver->LowLevelSendToChannelId.Get(), MessageType_LOW_LEVEL, DataToSend, DataSize);
			}

		}
	}
	//auto ChanneldDriver = CastChecked<UChanneldNetDriver>(Driver);
	//ChanneldDriver->LowLevelSend(RemoteAddr, Data, CountBits, Traits);
}

FString UChanneldNetConnection::LowLevelGetRemoteAddress(bool bAppendPort /*= false*/)
{
	if (RemoteAddr)
	{
		return RemoteAddr->ToString(bAppendPort);
	}
	else
	{
		return FString::Printf(TEXT("0.0.0.0%s"), bAppendPort ? ":0" : "");
	}
}

FString UChanneldNetConnection::LowLevelDescribe()
{
	return FString::Printf
	(
		TEXT("connId: %d, state: %s"),
		GetConnId(),
		State == USOCK_Pending ? TEXT("Pending")
		: State == USOCK_Open ? TEXT("Open")
		: State == USOCK_Closed ? TEXT("Closed")
		: TEXT("Invalid")
	);
}

void UChanneldNetConnection::Tick(float DeltaSeconds)
{
	UNetConnection::Tick(DeltaSeconds);
}

void UChanneldNetConnection::FlushUnauthData()
{
	while (!LowLevelSendDataBeforeAuth.IsEmpty())
	{
		TTuple<std::string*, FOutPacketTraits*> Params;
		LowLevelSendDataBeforeAuth.Dequeue(Params);
		std::string* data = Params.Get<0>();
		LowLevelSend((uint8*)data->data(), data->size() * 8, *Params.Get<1>());
		UE_LOG(LogChanneld, Log, TEXT("NetConnection %d flushed unauthenticated LowLevelSendData to channeld, size: %dB"), GetConnId(), data->size());
		delete data;
	}
}

void UChanneldNetConnection::ReceivedRawPacket(void* Data, int32 Count)
{
	if (Count == 0 || Driver == NULL)
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
				//UE_LOG(LogNet, Warning, TEXT("UChanneldNetConnection::bChallengeHandshake: %s"), *LowLevelDescribe());
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
