#pragma once
#include "AreaOfInterestBase.h"
#include "PlayerFollowingAOI.generated.h"

UCLASS(Abstract)
class CHANNELDUE_API UPlayerFollowingAOI : public UAreaOfInterestBase
{
	GENERATED_BODY()
public:
	
	virtual void FollowActor(AActor* Target) override;

	virtual void UnfollowActor(AActor* Target) override;

	virtual bool TickQuery(channeldpb::SpatialInterestQuery* Query, float DeltaTime) override;
	
protected:
	TWeakObjectPtr<APlayerController> FollowingPC;
	FVector LastUpdateLocation;

};