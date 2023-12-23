#include "MetricsSubsystem.h"

#include "LogMetricsOutputDevice.h"

DEFINE_LOG_CATEGORY(LogPrometheus)

/*
void UCounter::BeginDestroy()
{
	UObject::BeginDestroy();
	
	if (auto FamilyObj = Cast<UCounterFamily>(GetOuter()))
	{
		if (FamilyObj->Family)
		{
			FamilyObj->Family->Remove(Counter);
		}
	}
	
	Counter = nullptr;
}

void UCounterFamily::BeginDestroy()
{
	UObject::BeginDestroy();

	for (auto& CounterObj : CounterObjs)
	{
		CounterObj->ConditionalBeginDestroy();
	}
	CounterObjs.Empty();
}
*/

void UGauge::BeginDestroy()
{
	UObject::BeginDestroy();
	
	if (auto FamilyObj = Cast<UGaugeFamily>(GetOuter()))
	{
		if (FamilyObj->Family)
		{
			FamilyObj->Family->Remove(Gauge);
		}
	}
	
	Gauge = nullptr;
}

void UMetricsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	RegistryPtr = std::make_shared<Registry>();

	auto CmdLine = FCommandLine::Get();
	bool bMetricsEnabled = false;
	if (FParse::Bool(CmdLine, TEXT("metrics"), bMetricsEnabled))
	{
		bMetricsEnabled = true;
		UE_LOG(LogPrometheus, Log, TEXT("Parsed bMetricsEnabled from CLI: %d"), bMetricsEnabled);
	}
	if (FParse::Value(CmdLine, TEXT("prometheusPort="), ExposerPort))
	{
		UE_LOG(LogPrometheus, Log, TEXT("Parsed Prometheus Port from CLI: %d"), ExposerPort);
	}
	int32 MetricsLogLevel = ELogVerbosity::Warning;
	if (FParse::Value(CmdLine, TEXT("metricsLogLevel="), MetricsLogLevel))
	{
		UE_LOG(LogPrometheus, Log, TEXT("Parsed MetricsLogLevel from CLI: %d"), MetricsLogLevel);
	}
	if (bMetricsEnabled && MetricsLogLevel > 0)
	{
		auto LogMetricsFamily = &AddCounterFamily(FName("ue_logs"), TEXT("Number of logs in UE"));
		LogMetricsCounterFamily = TSharedPtr<Family<Counter>>(LogMetricsFamily);
		LogMetricsOutputDevice = MakeShared<FLogMetricsOutputDevice>(*LogMetricsFamily);
		LogMetricsOutputDevice->ThresholdVerbosity = static_cast<ELogVerbosity::Type>(MetricsLogLevel);
		FOutputDeviceRedirector::Get()->AddOutputDevice(LogMetricsOutputDevice.Get());
	}

	if (bMetricsEnabled)
	{
		ExposerPtr = MakeShared<Exposer>("0.0.0.0:" + std::to_string(ExposerPort));
		ExposerPtr->RegisterCollectable(RegistryPtr);
	}
}

void UMetricsSubsystem::Deinitialize()
{
	if (LogMetricsOutputDevice.IsValid())
	{
		FOutputDeviceRedirector::Get()->RemoveOutputDevice(LogMetricsOutputDevice.Get());
	}
	if (LogMetricsCounterFamily.IsValid())
	{
		Remove(*LogMetricsCounterFamily);
	}
	
	for (auto& Pair : CounterFamilies)
	{
		// Trigger UCounterFamily::BeginDestroy() to remove all counters from the family
		Pair.Value->ConditionalBeginDestroy();
		RegistryPtr->Remove(*Pair.Value->Family);
	}
	CounterFamilies.Empty();
	
	for (auto& Pair : GaugeFamilies)
	{
		// Trigger UGaugeFamily::BeginDestroy() to remove all gauges from the family
		Pair.Value->ConditionalBeginDestroy();
		RegistryPtr->Remove(*Pair.Value->Family);
	}
	GaugeFamilies.Empty();
	
	if (ExposerPtr)
	{
		ExposerPtr->RemoveCollectable(RegistryPtr);
	}
}

Family<Counter>& UMetricsSubsystem::AddCounterFamily(const FName& Name, const FString& Help)
{
	return BuildCounter()
		.Name(std::string(TCHAR_TO_UTF8(*Name.ToString())))
		.Help(std::string(TCHAR_TO_UTF8(*Help)))
		.Register(*RegistryPtr);
}

Family<Gauge>& UMetricsSubsystem::AddGaugeFamily(const FName& Name, const FString& Help)
{
	return BuildGauge()
		.Name(std::string(TCHAR_TO_UTF8(*Name.ToString())))
		.Help(std::string(TCHAR_TO_UTF8(*Help)))
		.Register(*RegistryPtr);
}

void UMetricsSubsystem::Remove(const Family<Counter>& CounterFamily)
{
	RegistryPtr->Remove(CounterFamily);
}

void UMetricsSubsystem::Remove(const Family<Gauge>& GaugeFamily)
{
	RegistryPtr->Remove(GaugeFamily);
}
