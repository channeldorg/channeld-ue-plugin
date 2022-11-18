#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ChanneldCharacter.generated.h"

// Responsible for replacing the default CharacterMovementComponent with the customized one.
UCLASS(BlueprintType)
class AChanneldCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AChanneldCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};