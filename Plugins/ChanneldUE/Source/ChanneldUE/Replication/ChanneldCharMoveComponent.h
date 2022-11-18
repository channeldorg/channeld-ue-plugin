#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ChanneldCharMoveComponent.generated.h"

// Responsible for serializing ClientAdjustment.NewBase properly.
struct FChanneldCharacterMoveResponseDataContainer : FCharacterMoveResponseDataContainer
{
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap) override;
};

// Responsible for customizing FCharacterMoveResponseDataContainer::Serialize.
UCLASS(BlueprintType, meta = (DisplayName = "Channeld Character Movement Component", BlueprintSpawnableComponent))
class CHANNELDUE_API UChanneldCharMoveComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()
	
public:
	UChanneldCharMoveComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	FChanneldCharacterMoveResponseDataContainer DefaultMoveResponseDataContainer;
};