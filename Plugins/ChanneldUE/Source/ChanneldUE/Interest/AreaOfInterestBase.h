#pragma once

#include "CoreMinimal.h"
#include "ChanneldNetConnection.h"
#include "AreaOfInterestBase.generated.h"

UCLASS(Abstract)
class CHANNELDUE_API UAreaOfInterestBase : public UObject
{
	GENERATED_BODY()
	
public:
	UAreaOfInterestBase() {}

	virtual void OnActivate() {}
	virtual void OnDeactivate() {}
	// virtual void OnPlayerEnterSpatialChannel(UChanneldNetConnection* NetConn, Channeld::ChannelId SpatialChId) PURE_VIRTUAL(,)
	virtual void SetSpatialQuery(channeldpb::SpatialInterestQuery* Query, const FVector& PawnLocation, const FRotator& PawnRotation) PURE_VIRTUAL(,);
	virtual void FollowActor(AActor* Target) {}
	virtual void UnfollowActor(AActor* Target) {}
	virtual bool TickQuery(channeldpb::SpatialInterestQuery* Query, float DeltaTime) {return false;}

	UPROPERTY(BlueprintReadOnly)
	FName Name;

	UPROPERTY(EditAnywhere)
	float MinDistanceToTriggerUpdate;
};
