#include "Metrics.h"
#include "channeld.pb.h"
#include "ChanneldConnection.h"

void UMetrics::Initialize(FSubsystemCollectionBase& Collection)
{
	auto CmdLine = FCommandLine::Get();
	bool bMetricsEnabled = false;
	if (FParse::Bool(CmdLine, TEXT("metrics"), bMetricsEnabled))
	{
		bMetricsEnabled = true;
		UE_LOG(LogChanneld, Log, TEXT("Parsed bMetricsEnabled from CLI: %d"), bMetricsEnabled);
	}
	if (FParse::Value(CmdLine, TEXT("prometheusPort="), ExposerPort))
	{
		UE_LOG(LogChanneld, Log, TEXT("Parsed Prometheus Port from CLI: %d"), ExposerPort);
	}

	registry = std::make_shared<Registry>();
	if (bMetricsEnabled)
	{
		exposer = MakeShared<Exposer>("0.0.0.0:" + std::to_string(ExposerPort));
		exposer->RegisterCollectable(registry);
	}

	FPS = &AddGauge(FName("ue_server_fps"), TEXT("Framerate of the UE server"));
	ReplicatedProviders = &AddCounter(FName("ue_provider_reps"), TEXT("Number of the replicated data providers"));
	SentRPCs = &AddCounter(FName("ue_sent_rpcs"), TEXT("Number of the RPCs sent via channeld"));
}

void UMetrics::Deinitialize()
{
	if (exposer)
	{
		exposer->RemoveCollectable(registry);
	}
}

void UMetrics::Tick(float DeltaTime)
{
	AddConnTypeLabel(*FPS).Set(1.0 / DeltaTime);
}

Family<Counter>& UMetrics::AddCounter(const FName& Name, const FString& Help)
{
	return BuildCounter()
		.Name(std::string(TCHAR_TO_UTF8(*Name.ToString())))
		.Help(std::string(TCHAR_TO_UTF8(*Help)))
		.Register(*registry);
}

Family<Gauge>& UMetrics::AddGauge(const FName& Name, const FString& Help)
{
	return BuildGauge()
		.Name(std::string(TCHAR_TO_UTF8(*Name.ToString())))
		.Help(std::string(TCHAR_TO_UTF8(*Help)))
		.Register(*registry);
}

Counter& UMetrics::AddConnTypeLabel(Family<Counter>& Family)
{
	return Family.Add({ { "connType", channeldpb::ConnectionType_Name(GEngine->GetEngineSubsystem<UChanneldConnection>()->GetConnectionType()) } });
}

Gauge& UMetrics::AddConnTypeLabel(Family<Gauge>& Family)
{
	return Family.Add({ { "connType", channeldpb::ConnectionType_Name(GEngine->GetEngineSubsystem<UChanneldConnection>()->GetConnectionType()) } });
}
