#pragma once

#include "PlayerFollowingAOI.h"
#include "BoxAOI.generated.h"

UCLASS(BlueprintType)
class CHANNELDUE_API UBoxAOI : public UPlayerFollowingAOI
{
	GENERATED_BODY()
	
public:
	
	virtual void SetSpatialQuery(channeldpb::SpatialInterestQuery* Query, const FVector& PawnLocation, const FRotator& PawnRotation) override;

	UPROPERTY(EditAnywhere)
	FVector Extent;
};