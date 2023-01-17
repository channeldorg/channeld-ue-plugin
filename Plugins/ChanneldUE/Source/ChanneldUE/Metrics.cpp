#include "Metrics.h"
#include "channeld.pb.h"
#include "ChanneldConnection.h"
#include "ChanneldTypes.h"

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
	CPU = &AddGauge(FName("ue_server_cpu"), TEXT("CPU usage of the UE server in percentage"));
	Memory = &AddGauge(FName("ue_server_mem"), TEXT("Memory usage of the UE server in MB"));
	UnfinishedPacket = &AddCounter(FName("ue_packets_frag"), TEXT("Number of fragmented packets received"));
	DroppedPacket = &AddCounter(FName("ue_packets_drop"), TEXT("Number of dropped packets received"));
	ReplicatedProviders = &AddCounter(FName("ue_provider_reps"), TEXT("Number of the replicated data providers"));
	SentRPCs = &AddCounter(FName("ue_sent_rpcs"), TEXT("Number of the RPCs sent via channeld"));
	GetHandoverContexts = &AddCounter(FName("ue_handover_ctx"), TEXT("Number of getting handover context"));
	Handovers = &AddCounter(FName("ue_handovers"), TEXT("Number of handovers"));
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
	// AddConnTypeLabel(*FPS).Set(1.0 / DeltaTime);
	FPS->Add({}).Set(1.0 / DeltaTime);
	CPU->Add({}).Set(FPlatformTime::GetCPUTime().CPUTimePct);
	Memory->Add({}).Set(FPlatformMemory::GetStats().UsedPhysical >> 20);
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
