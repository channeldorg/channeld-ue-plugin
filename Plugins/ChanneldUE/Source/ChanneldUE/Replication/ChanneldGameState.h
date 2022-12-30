#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ChanneldGameState.generated.h"

UCLASS(BlueprintType)
class AChanneldGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual int32 GetFunctionCallspace(UFunction* Function, FFrame* Stack) override;
};