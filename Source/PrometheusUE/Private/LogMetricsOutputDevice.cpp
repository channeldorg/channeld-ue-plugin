#include "LogMetricsOutputDevice.h"

void FLogMetricsOutputDevice::Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category)
{
	if (Verbosity > ThresholdVerbosity)
	{
		return;
	}

	TTuple<FName, ELogVerbosity::Type> Key(Category, Verbosity);
	if (Counter* Ctr = CountersByCategory.FindRef(Key))
	{
		Ctr->Increment();
	}
	else
	{
		Counter* NewCounter = &Family->Add({
			{"category", TCHAR_TO_UTF8(*Category.ToString())},
			{"level", TCHAR_TO_UTF8(ToString(Verbosity))},
			{"node", TCHAR_TO_UTF8(FPlatformProcess::ComputerName())},
		});
		CountersByCategory.Add(Key, NewCounter);
		NewCounter->Increment();
	}
}
