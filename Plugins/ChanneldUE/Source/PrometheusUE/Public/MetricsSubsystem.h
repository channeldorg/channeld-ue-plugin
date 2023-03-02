#pragma once

#include "CoreMinimal.h"
#include "prometheus/counter.h"
#include "prometheus/gauge.h"
#include "prometheus/exposer.h"
#include "prometheus/registry.h"
#include "MetricsSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPrometheus, Log, All);

using namespace prometheus;

UCLASS(BlueprintType)
class PROMETHEUSUE_API UCounter : public UObject
{
	GENERATED_BODY()

public:

	Counter* Counter;

	UFUNCTION(BlueprintCallable, Category = "Prometheus|Counter")
	void Increment(const float Value = 1.0)
	{
		if (Counter)
		{
			Counter->Increment(Value);
		}
	}

	/* The UCounter objects will be destroyed by the GC, but we don't want to remove the Counter from the Family,
	 * as that will cause the Counter value to be reset.
	virtual void BeginDestroy() override;
	*/
};

UCLASS(BlueprintType)
class PROMETHEUSUE_API UCounterFamily : public UObject
{
	GENERATED_BODY()

public:
	
	Family<Counter>* Family;

	/* Do not store the counter UObjects - they should be destroyed by the GC
	UPROPERTY()
	TArray<UCounter*> CounterObjs;

	virtual void BeginDestroy() override;
	*/
	
	UFUNCTION(BlueprintCallable, Category = "Prometheus|Counter")
	UCounter* AddCounter(const FName LabelName, const FString& Value)
	{
		auto CounterObj = NewObject<UCounter>(this);
		CounterObj->Counter = &Family->Add({{TCHAR_TO_UTF8(*LabelName.ToString()), TCHAR_TO_UTF8(*Value)}});
		// CounterObjs.Add(CounterObj);
		return CounterObj;
	}

	UFUNCTION(BlueprintCallable, Category = "Prometheus|Counter")
	UCounter* AddDefaultCounter()
	{
		auto CounterObj = NewObject<UCounter>(this);
		CounterObj->Counter = &Family->Add({});
		// CounterObjs.Add(CounterObj);
		return CounterObj;
	}

	UFUNCTION(BlueprintCallable, Category = "Prometheus|Counter")
	UCounter* AddCounterWithLabels(const TMap<FName, FString>& Labels)
	{
		auto CounterObj = NewObject<UCounter>(this);
		std::map<std::string, std::string> LabelMap;
		for (const auto& Label : Labels)
		{
			LabelMap.insert({TCHAR_TO_UTF8(*Label.Key.ToString()), TCHAR_TO_UTF8(*Label.Value)});
		}
		CounterObj->Counter = &Family->Add(LabelMap);
		// CounterObjs.Add(CounterObj);
		return CounterObj;
	}
};

UCLASS(BlueprintType)
class PROMETHEUSUE_API UGauge : public UObject
{
	GENERATED_BODY()

public:

	Gauge* Gauge;

	UFUNCTION(BlueprintCallable, Category = "Prometheus|Gauge")
	void Set(const float Value)
	{
		if (Gauge)
		{
			Gauge->Set(Value);
		}
	}

	virtual void BeginDestroy() override;
};

UCLASS(BlueprintType)
class PROMETHEUSUE_API UGaugeFamily : public UObject
{
	GENERATED_BODY()

public:
	
	Family<Gauge>* Family;

	UFUNCTION(BlueprintCallable, Category = "Prometheus|Gauge")
	UGauge* AddGauge(const FName LabelName, const FString& Value)
	{
		auto GaugeObj = NewObject<UGauge>(this);
		GaugeObj->Gauge = &Family->Add({{TCHAR_TO_UTF8(*LabelName.ToString()), TCHAR_TO_UTF8(*Value)}});
		return GaugeObj;
	}

	UFUNCTION(BlueprintCallable, Category = "Prometheus|Gauge")
	UGauge* AddDefaultGauge()
	{
		auto GaugeObj = NewObject<UGauge>(this);
		GaugeObj->Gauge = &Family->Add({});
		return GaugeObj;
	}

	UFUNCTION(BlueprintCallable, Category = "Prometheus|Gauge")
	UGauge* AddGaugeWithLabels(const TMap<FName, FString>& Labels)
	{
		auto GaugeObj = NewObject<UGauge>(this);
		std::map<std::string, std::string> LabelMap;
		for (const auto& Label : Labels)
		{
			LabelMap.insert({TCHAR_TO_UTF8(*Label.Key.ToString()), TCHAR_TO_UTF8(*Label.Value)});
		}
		GaugeObj->Gauge = &Family->Add(LabelMap);
		return GaugeObj;
	}
};

UCLASS(Transient, config = Engine)
class PROMETHEUSUE_API UMetricsSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	Family<Counter>& AddCounterFamily(const FName& Name, const FString& Help);
	Family<Gauge>& AddGaugeFamily(const FName& Name, const FString& Help);
	void Remove(const Family<Counter>& CounterFamily);
	void Remove(const Family<Gauge>& GaugeFamily);

	UFUNCTION(BlueprintCallable, Category = "Prometheus", meta=(DisplayName="AddCounterFamily", ScriptName="AddCounterFamily"))
	UCounterFamily* K2_AddCounterFamily(const FName Name, const FString& Help)
	{
		auto FamilyObj = GetCounterFamily(Name);
		if (FamilyObj)
		{
			return FamilyObj;
		}
		
		FamilyObj = NewObject<UCounterFamily>(this);
		FamilyObj->Family = &AddCounterFamily(Name, Help);
		CounterFamilies.Add(Name, FamilyObj);
		return FamilyObj;
	}

	UFUNCTION(BlueprintCallable, Category = "Prometheus")
	UCounterFamily* GetCounterFamily(const FName Name)
	{
		return CounterFamilies.FindRef(Name);
	}

	UFUNCTION(BlueprintCallable, Category = "Prometheus", meta=(DisplayName="AddGaugeFamily", ScriptName="AddGaugeFamily"))
	UGaugeFamily* K2_AddGaugeFamily(const FName Name, const FString& Help)
	{
		auto FamilyObj = GetGaugeFamily(Name);
		if (FamilyObj)
		{
			return FamilyObj;
		}
		
		FamilyObj = NewObject<UGaugeFamily>(this);
		FamilyObj->Family = &AddGaugeFamily(Name, Help);
		GaugeFamilies.Add(Name, FamilyObj);
		return FamilyObj;
	}

	UFUNCTION(BlueprintCallable, Category = "Prometheus")
	UGaugeFamily* GetGaugeFamily(const FName Name)
	{
		return GaugeFamilies.FindRef(Name);
	}

	UPROPERTY(Config)
	int32 ExposerPort = 8081;

private:

	TSharedPtr<Exposer> ExposerPtr;
	std::shared_ptr<Registry> RegistryPtr;

	UPROPERTY()
	TMap<FName, UCounterFamily*> CounterFamilies;
	UPROPERTY()
	TMap<FName, UGaugeFamily*> GaugeFamilies;
};