#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ChanneldCharMoveComponent.generated.h"

// Responsible for serializing ClientAdjustment.NewBase properly.
struct FChanneldCharacterMoveResponseDataContainer : FCharacterMoveResponseDataContainer
{
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap) override;
};

// Responsibilities:
// 1. Customize the serialization of FCharacterMoveResponseDataContainer (disabled for now)
// 2. Adapt the cross-server handover situation
UCLASS(BlueprintType, meta = (DisplayName = "Channeld Character Movement Component", BlueprintSpawnableComponent))
class CHANNELDUE_API UChanneldCharMoveComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()
	
public:
	UChanneldCharMoveComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual bool ForcePositionUpdate(float DeltaTime) override;
	
protected:
	FChanneldCharacterMoveResponseDataContainer DefaultMoveResponseDataContainer;
};