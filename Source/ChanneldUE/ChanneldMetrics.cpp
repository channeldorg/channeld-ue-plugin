#include "ChanneldMetrics.h"

#include "channeld.pb.h"
#include "ChanneldConnection.h"
#include "ChanneldSettings.h"

void UChanneldMetrics::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UMetricsSubsystem::StaticClass());

	FString MetricsName;
	if (FParse::Value(FCommandLine::Get(), TEXT("MetricsName="), MetricsName))
	{
		UE_LOG(LogChanneld, Log, TEXT("Parsed MetricsName from CLI: %s"), *MetricsName);
	}
	else
	{
		MetricsName = GetMutableDefault<UChanneldSettings>()->ChannelDataViewClass->GetName();
	}
	
	const Labels NameLabel = {{ "name", TCHAR_TO_UTF8(*MetricsName) }};
	auto Metrics = GEngine->GetEngineSubsystem<UMetricsSubsystem>();
	
	FPS = &Metrics->AddGaugeFamily(FName("ue_server_fps"), TEXT("Framerate of the UE server"));
	FPS_Gauge = &FPS->Add(NameLabel);
	
	CPU = &Metrics->AddGaugeFamily(FName("ue_server_cpu"), TEXT("CPU usage of the UE server in percentage"));
	CPU_Gauge = &CPU->Add(NameLabel);

	Memory = &Metrics->AddGaugeFamily(FName("ue_server_mem"), TEXT("Memory usage of the UE server in MB"));
	MEM_Gauge = &Memory->Add(NameLabel);

	FragmentedPacket = &Metrics->AddCounterFamily(FName("ue_packets_frag"), TEXT("Number of fragmented packets received"));
	FragmentedPacket_Counter = &FragmentedPacket->Add(NameLabel);
	
	DroppedPacket = &Metrics->AddCounterFamily(FName("ue_packets_drop"), TEXT("Number of dropped packets"));
	DroppedPacket_Counter = &DroppedPacket->Add(NameLabel);
	
	ReplicatedProviders = &Metrics->AddCounterFamily(FName("ue_provider_reps"), TEXT("Number of the replicated data providers"));
	ReplicatedProviders_Counter = &ReplicatedProviders->Add(NameLabel);

	SentRPCs = &Metrics->AddCounterFamily(FName("ue_rpc_sent"), TEXT("Number of the RPCs sent to channeld"));
	SentRPCs_Counter = &SentRPCs->Add(NameLabel);
	
	ReceivedRPCs = &Metrics->AddCounterFamily(FName("ue_rpc_recv"), TEXT("Number of the RPCs received from channeld"));
	ReceivedRPCs_Counter = &ReceivedRPCs->Add(NameLabel);

	DroppedRPCs = &Metrics->AddCounterFamily(FName("ue_rpc_drop"), TEXT("Number of the RPCs dropped"));
	DroppedRPCs_Counter = &DroppedRPCs->Add(NameLabel);

	RedirectedRPCs = &Metrics->AddCounterFamily(FName("ue_rpc_redir"), TEXT("Number of the RPCs redirected"));
	RedirectedRPCs_Counter = &RedirectedRPCs->Add(NameLabel);
	
	Handovers = &Metrics->AddCounterFamily(FName("ue_handovers"), TEXT("Number of handovers"));
}

void UChanneldMetrics::Deinitialize()
{
	auto Metrics = GEngine->GetEngineSubsystem<UMetricsSubsystem>();
	FPS->Remove(FPS_Gauge);
	Metrics->Remove(*FPS);
	
	CPU->Remove(CPU_Gauge);
	Metrics->Remove(*CPU);

	Memory->Remove(MEM_Gauge);
	Metrics->Remove(*Memory);

	FragmentedPacket->Remove(FragmentedPacket_Counter);
	Metrics->Remove(*FragmentedPacket);

	DroppedPacket->Remove(DroppedPacket_Counter);
	Metrics->Remove(*DroppedPacket);

	ReplicatedProviders->Remove(ReplicatedProviders_Counter);
	Metrics->Remove(*ReplicatedProviders);

	SentRPCs->Remove(SentRPCs_Counter);
	Metrics->Remove(*SentRPCs);

	ReceivedRPCs->Remove(ReceivedRPCs_Counter);
	Metrics->Remove(*ReceivedRPCs);

	DroppedRPCs->Remove(DroppedRPCs_Counter);
	Metrics->Remove(*DroppedRPCs);

	RedirectedRPCs->Remove(RedirectedRPCs_Counter);
	Metrics->Remove(*RedirectedRPCs);
	
	Metrics->Remove(*Handovers);
}

void UChanneldMetrics::Tick(float DeltaTime)
{
	FPS_Gauge->Set(1.0 / DeltaTime);
	CPU_Gauge->Set(FPlatformTime::GetCPUTime().CPUTimePct);
	MEM_Gauge->Set(FPlatformMemory::GetStats().UsedPhysical >> 20);
}

void UChanneldMetrics::OnDroppedRPC(const std::string& FuncName, ERPCDropReason Reason)
{
	DroppedRPCs_Counter->Increment();
#if !UE_BUILD_SHIPPING
	DroppedRPCs->Add({{"funcName", FuncName}, {"reason", std::to_string(Reason)}}).Increment();
#endif
}
