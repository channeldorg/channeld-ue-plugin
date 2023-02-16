#pragma once

#include "AreaOfInterestBase.h"
#include "SphereAOI.generated.h"

UCLASS(BlueprintType)
class CHANNELDUE_API USphereAOI : public UAreaOfInterestBase
{
	GENERATED_BODY()
	
public:
	USphereAOI(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) {}

	UPROPERTY(EditAnywhere)
	float Radius;
	
protected:
	virtual void SetSpatialQuery(channeldpb::SpatialInterestQuery* Query, const FVector& PawnLocation, const FRotator& PawnRotation) override;
};