#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ChanneldGameState.generated.h"

// Responsibility: Route the process of any server UFunction to the the Master server.
// In order to make the replicated UProperties consistent across all servers, the Master server should be the only one that can update these properties,
// and the update of these properties should always be wrapped in a setter server UFunction.
UCLASS(BlueprintType)
class AChanneldGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual int32 GetFunctionCallspace(UFunction* Function, FFrame* Stack) override;
};