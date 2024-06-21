#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ChanneldCharMoveComponent.generated.h"

// Responsible for serializing ClientAdjustment.NewBase properly.
struct FChanneldCharacterMoveDataContainer_SC : FCharacterMoveResponseDataContainer
{
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap) override;
};

struct FChanneldCharacterNetworkMoveData : FCharacterNetworkMoveData
{
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType);
};

struct FChanneldCharacterMoveDataContainer_CS : FCharacterNetworkMoveDataContainer
{
	FChanneldCharacterMoveDataContainer_CS()
	{
		NewMoveData		= &DefaultMoveData[0];
		PendingMoveData	= &DefaultMoveData[1];
		OldMoveData		= &DefaultMoveData[2];
	}
private:
	FChanneldCharacterNetworkMoveData DefaultMoveData[3];
};


// Responsibilities:
// 1. Customize the serialization of MovementBase (disabled for now)
// 2. Adapt the cross-server handover situation
UCLASS(BlueprintType, meta = (DisplayName = "Channeld Character Movement Component", BlueprintSpawnableComponent))
class CHANNELDUE_API UChanneldCharMoveComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()
	
public:
	UChanneldCharMoveComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual bool ForcePositionUpdate(float DeltaTime) override;
	
protected:
	FChanneldCharacterMoveDataContainer_SC DefaultMoveDataContainer_SC;
	FChanneldCharacterMoveDataContainer_CS DefaultMoveDataContainer_CS;
};