#pragma once

#include "PlayerFollowingAOI.h"
#include "SphereAOI.generated.h"

UCLASS(BlueprintType)
class CHANNELDUE_API USphereAOI : public UPlayerFollowingAOI
{
	GENERATED_BODY()
	
public:
	// USphereAOI(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) {}
	
	virtual void SetSpatialQuery(channeldpb::SpatialInterestQuery* Query, const FVector& PawnLocation, const FRotator& PawnRotation) override;

	UPROPERTY(EditAnywhere)
	float Radius;
};