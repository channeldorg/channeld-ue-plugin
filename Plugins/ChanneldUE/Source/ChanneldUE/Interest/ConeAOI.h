#pragma once

#include "PlayerFollowingAOI.h"
#include "ConeAOI.generated.h"

UCLASS(BlueprintType)
class CHANNELDUE_API UConeAOI : public UPlayerFollowingAOI
{
	GENERATED_BODY()
	
public:
	virtual void FollowActor(AActor* Target) override;
	virtual void SetSpatialQuery(channeldpb::SpatialInterestQuery* Query, const FVector& PawnLocation, const FRotator& PawnRotation) override;
	virtual bool TickQuery(channeldpb::SpatialInterestQuery* Query, float DeltaTime) override;
	
	UPROPERTY(EditAnywhere)
	float Radius;

	UPROPERTY(EditAnywhere)
	float Angle;

protected:
	FRotator LastUpdateRotation;
};