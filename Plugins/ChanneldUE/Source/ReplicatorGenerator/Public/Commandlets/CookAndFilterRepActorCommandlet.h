#pragma once

#include "CoreMinimal.h"
#include "ChanneldCookCommandlet.h"
#include "CookAndFilterRepActorCommandlet.generated.h"

/**
 * 
 */
UCLASS()
class REPLICATORGENERATOR_API UCookAndFilterRepActorCommandlet : public UChanneldCookCommandlet
{
	GENERATED_BODY()

public:
	UCookAndFilterRepActorCommandlet();

	virtual int32 Main(const FString& CmdLineParams) override;

	static void LoadResult(TArray<FString>& Result, bool& Success);

	void SaveResult(TArray<FString>);
};
