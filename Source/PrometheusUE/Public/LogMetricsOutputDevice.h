#pragma once

#include "Misc/OutputDevice.h"
#include "prometheus/family.h"
#include "prometheus/counter.h"

using namespace prometheus;

class FLogMetricsOutputDevice : public FOutputDevice
{
public:
	FLogMetricsOutputDevice(Family<Counter>& InFamily) : Family(&InFamily) {}

	ELogVerbosity::Type ThresholdVerbosity = ELogVerbosity::Warning;
	
protected:
	Family<Counter>* Family;
	TMap<TTuple<FName, ELogVerbosity::Type>, Counter*> CountersByCategory;
	
	virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const class FName& Category) override;
};
