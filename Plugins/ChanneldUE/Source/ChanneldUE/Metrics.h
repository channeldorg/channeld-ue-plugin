#pragma once

#include "CoreMinimal.h"

#include "prometheus/counter.h"
#include "prometheus/gauge.h"
#include "prometheus/exposer.h"
#include "prometheus/registry.h"

#include "Metrics.generated.h"

using namespace prometheus;

UCLASS(transient, config = Engine)
class CHANNELDUE_API UMetrics : public UEngineSubsystem, public FTickableGameObject
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

	Family<Counter>& AddCounter(const FName& Name, const FString& Help);
	Family<Gauge>& AddGauge(const FName& Name, const FString& Help);

	Counter& AddConnTypeLabel(Family<Counter>& Family);
	Gauge& AddConnTypeLabel(Family<Gauge>& Family);

	UPROPERTY(Config, EditAnywhere)
	int32 ExposerPort = 8081;

	Family<Gauge>* FPS;
	Family<Counter>* ReplicatedProviders;
	Family<Counter>* SentRPCs;

private:

	TSharedPtr<Exposer> exposer;
	std::shared_ptr<Registry> registry;
};