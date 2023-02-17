#pragma once

#include "AreaOfInterestBase.h"
#include "StaticLocationsAOI.generated.h"

UCLASS(BlueprintType)
class CHANNELDUE_API UStaticLocationsAOI : public UAreaOfInterestBase
{
	GENERATED_BODY()
	
public:
	virtual void SetSpatialQuery(channeldpb::SpatialInterestQuery* Query, const FVector& PawnLocation, const FRotator& PawnRotation) override;

	UPROPERTY(EditAnywhere)
	TMap<FVector, uint32> SpotsAndDists;
};