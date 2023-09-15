#pragma once
#include "MetricsSubsystem.h"
#include "View/ChannelDataView.h"
#include "ChanneldMetrics.generated.h"

enum ERPCDropReason : uint8
{
	RPCDropReason_Unknown = 0,
	RPCDropReason_InvalidMulticast = 1,
	RPCDropReason_RedirMaxRetried = 2,
	RPCDropReason_RedirNoChannel = 3,
	RPCDropReason_RedirNoView = 4,
	RPCDropReason_SerializeFailed = 5,
	RPCDropReason_NoRepComp = 6,
	RPCDropReason_NoAuthority = 7,
	RPCDropReason_DeserializeFailed = 8,
};

UCLASS(transient)
class CHANNELDUE_API UChanneldMetrics : public UEngineSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
	
public:
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//~ Begin FTickableGameObject Interface.
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return !IsTemplate(); }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMetrics, STATGROUP_Tickables); }
	//~ End FTickableGameObject Interface
	
	void OnDroppedRPC(const std::string& String, ERPCDropReason Reason);
	
	Family<Gauge>* FPS;
	Gauge* FPS_Gauge;

	Family<Gauge>* CPU;
	Gauge* CPU_Gauge;

	Family<Gauge>* Memory;
	Gauge* MEM_Gauge;
	
	Family<Counter>* FragmentedPacket;
	Counter* FragmentedPacket_Counter;

	Family<Counter>* DroppedPacket;
	Counter* DroppedPacket_Counter;
	
	Family<Counter>* ReplicatedProviders;
	Counter* ReplicatedProviders_Counter;
	
	Family<Counter>* SentRPCs;
	Counter* SentRPCs_Counter;
	
	Family<Counter>* ReceivedRPCs;
	Counter* ReceivedRPCs_Counter;

	Family<Counter>* DroppedRPCs;
	Counter* DroppedRPCs_Counter;

	Family<Counter>* RedirectedRPCs;
	Counter* RedirectedRPCs_Counter;
	
	Family<Counter>* Handovers;
};
