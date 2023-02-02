#pragma once

#include "CoreMinimal.h"
#include "ATintActor.generated.h"

UCLASS()
class ATintActor : public AActor
{
	GENERATED_BODY()
public:
	ATintActor(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintImplementableEvent)
	void SetColor(FLinearColor Color);
};
