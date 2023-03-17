#include "ChanneldMetrics.h"

#include "channeld.pb.h"
#include "ChanneldConnection.h"

void UChanneldMetrics::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UMetricsSubsystem::StaticClass());

	auto Metrics = GEngine->GetEngineSubsystem<UMetricsSubsystem>();
	FPS = &Metrics->AddGaugeFamily(FName("ue_server_fps"), TEXT("Framerate of the UE server"));
	CPU = &Metrics->AddGaugeFamily(FName("ue_server_cpu"), TEXT("CPU usage of the UE server in percentage"));
	Memory = &Metrics->AddGaugeFamily(FName("ue_server_mem"), TEXT("Memory usage of the UE server in MB"));
	UnfinishedPacket = &Metrics->AddCounterFamily(FName("ue_packets_frag"), TEXT("Number of fragmented packets received"));
	DroppedPacket = &Metrics->AddCounterFamily(FName("ue_packets_drop"), TEXT("Number of dropped packets received"));
	ReplicatedProviders = &Metrics->AddCounterFamily(FName("ue_provider_reps"), TEXT("Number of the replicated data providers"));
	SentRPCs = &Metrics->AddCounterFamily(FName("ue_sent_rpcs"), TEXT("Number of the RPCs sent via channeld"));
	GetHandoverContexts = &Metrics->AddCounterFamily(FName("ue_handover_ctx"), TEXT("Number of getting handover context"));
	Handovers = &Metrics->AddCounterFamily(FName("ue_handovers"), TEXT("Number of handovers"));

	FPS_Gauge = &AddConnTypeLabel(FPS);
	CPU_Gauge = &AddConnTypeLabel(CPU);
	MEM_Gauge = &AddConnTypeLabel(Memory);
}

void UChanneldMetrics::Deinitialize()
{
	FPS->Remove(FPS_Gauge);
	CPU->Remove(CPU_Gauge);
	Memory->Remove(MEM_Gauge);
	
	auto Metrics = GEngine->GetEngineSubsystem<UMetricsSubsystem>();
	Metrics->Remove(*FPS);
	Metrics->Remove(*CPU);
	Metrics->Remove(*Memory);
	Metrics->Remove(*UnfinishedPacket);
	Metrics->Remove(*DroppedPacket);
	Metrics->Remove(*ReplicatedProviders);
	Metrics->Remove(*SentRPCs);
	Metrics->Remove(*GetHandoverContexts);
	Metrics->Remove(*Handovers);
}

void UChanneldMetrics::Tick(float DeltaTime)
{
	FPS_Gauge->Set(1.0 / DeltaTime);
	CPU_Gauge->Set(FPlatformTime::GetCPUTime().CPUTimePct);
	MEM_Gauge->Set(FPlatformMemory::GetStats().UsedPhysical >> 20);
}

Counter& UChanneldMetrics::AddConnTypeLabel(Family<Counter>* Family)
{
	return Family->Add({
		{ "connType", channeldpb::ConnectionType_Name(GEngine->GetEngineSubsystem<UChanneldConnection>()->GetConnectionType()) }
	});
}

Gauge& UChanneldMetrics::AddConnTypeLabel(Family<Gauge>* Family)
{
	return Family->Add({
		{ "connType", channeldpb::ConnectionType_Name(GEngine->GetEngineSubsystem<UChanneldConnection>()->GetConnectionType()) }
	});
}
