#include "ChanneldWorldSettings.h"

AChanneldWorldSettings::AChanneldWorldSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ReplicationComponent = CreateDefaultSubobject<UChanneldReplicationComponent>(TEXT("ChanneldReplication"));
}

TArray<FString> AChanneldWorldSettings::MergeLaunchParameters(const TArray<FString>& InParams) const
{
	TMap<FString, FString> ParamMap;
	for (const FString& Param : InParams)
	{
		FString Key;
		FString Value;
		if (Param.Split(TEXT("="), &Key, &Value))
		{
			ParamMap.Add(Key, Value);
		}
		else
		{
			ParamMap.Add(Param, TEXT(""));
		}
	}

	for (const FString& Param : ChanneldLaunchParametersOverride)
	{
		FString Key;
		FString Value;
		if (Param.Split(TEXT("="), &Key, &Value))
		{
			ParamMap.Emplace(Key, Value);
		}
		else
		{
			ParamMap.Emplace(Param, TEXT(""));
		}
	}

	TArray<FString> OutParams;
	for (const auto& Pair : ParamMap)
	{
		if (Pair.Value.IsEmpty())
		{
			OutParams.Add(Pair.Key);
		}
		else
		{
			OutParams.Add(Pair.Key + TEXT("=") + Pair.Value);
		}
	}
	return OutParams;
}
