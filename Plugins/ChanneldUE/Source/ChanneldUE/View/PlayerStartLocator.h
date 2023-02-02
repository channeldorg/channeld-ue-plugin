#pragma once

#include "CoreMinimal.h"
#include "PlayerStartLocator.generated.h"

UCLASS(Blueprintable)
class UPlayerStartLocatorBase : public UObject
{
	GENERATED_BODY()

public:
	UPlayerStartLocatorBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Default implementation: use the first PlayerStart found in the world. If non-existent, return the zero vector.
	UFUNCTION(BlueprintNativeEvent)
	FVector GetPlayerStartPosition(int64 ConnId, AActor*& StartSpot) const;

	virtual FVector GetPlayerStartPosition_Implementation(int64 ConnId, AActor*& StartSpot) const;
};

UCLASS()
class UPlayerStartLocator_ModByConnId : public UPlayerStartLocatorBase
{
	GENERATED_BODY()
public:
	UPlayerStartLocator_ModByConnId(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual FVector GetPlayerStartPosition_Implementation(int64 ConnId, AActor*& StartSpot) const override;
};
