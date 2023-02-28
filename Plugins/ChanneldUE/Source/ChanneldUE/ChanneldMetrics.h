#pragma once
#include "MetricsSubsystem.h"
#include "ChanneldMetrics.generated.h"

UCLASS(transient)
class CHANNELDUE_API UChanneldMetrics : public UEngineSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
	
public:
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//~ Begin FTickableGameObject Interface.
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return !IsTemplate(); };
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMetrics, STATGROUP_Tickables); }
	//~ End FTickableGameObject Interface

	Counter& AddConnTypeLabel(Family<Counter>* Family);
	Gauge& AddConnTypeLabel(Family<Gauge>* Family);
	
	Family<Gauge>* FPS;
	Family<Gauge>* CPU;
	Family<Gauge>* Memory;
	Gauge* FPS_Gauge;
	Gauge* CPU_Gauge;
	Gauge* MEM_Gauge;
	Family<Counter>* UnfinishedPacket;
	Family<Counter>* DroppedPacket;
	Family<Counter>* ReplicatedProviders;
	Family<Counter>* SentRPCs;
	Family<Counter>* GetHandoverContexts;
	Family<Counter>* Handovers;
};

