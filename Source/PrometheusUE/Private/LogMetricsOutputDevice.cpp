#include "LogMetricsOutputDevice.h"

void FLogMetricsOutputDevice::Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category)
{
	if (Verbosity > ThresholdVerbosity)
	{
		return;
	}

	if (Counter* Ctr = CountersByCategory.FindRef(Category))
	{
		Ctr->Increment();
	}
	else
	{
		Counter* NewCounter = &Family->Add({
			{"category", TCHAR_TO_UTF8(*Category.ToString())},
			{"node", TCHAR_TO_UTF8(FPlatformProcess::ComputerName())}
		});
		CountersByCategory.Add(Category, NewCounter);
		NewCounter->Increment();
	}
}
