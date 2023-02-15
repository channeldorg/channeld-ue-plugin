#pragma once

#include "CoreMinimal.h"
#include "ChanneldNetConnection.h"

class CHANNELDUE_API FAreaOfInterestBase// : public UObject
{
// GENERATED_BODY()
public:
	FAreaOfInterestBase() {}
	virtual ~FAreaOfInterestBase() {FollowingPC = nullptr;}

	virtual void OnActivate() {}
	virtual void OnDeactivate() {}
	virtual void SetSpatialQuery(channeldpb::SpatialInterestQuery* Query, const FVector& PawnLocation, const FRotator& PawnRotation) = 0;
	void FollowPlayer(APlayerController* PC) {FollowingPC = PC;}
	void UnfollowPlayer() {FollowingPC = nullptr;}
	virtual bool IsTickable() {return FollowingPC.IsValid();}
	virtual void Tick(float DeltaTime){}

	FName Name;
	
protected:
	
	TWeakObjectPtr<APlayerController> FollowingPC;
};
