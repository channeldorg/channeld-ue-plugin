#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ChanneldCharacter.generated.h"

// Responsibilities:
// 1. Replace the default CharacterMovementComponent with the customized UChanneldCharMoveComponent
// 2. Route server's ProcessEvent to the cross-server RPC if no authority over the character
// 3. Fix certain cross-server movement issues
UCLASS(BlueprintType)
class AChanneldCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AChanneldCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual int32 GetFunctionCallspace(UFunction* Function, FFrame* Stack) override;
	virtual void PostNetReceiveLocationAndRotation() override;
};