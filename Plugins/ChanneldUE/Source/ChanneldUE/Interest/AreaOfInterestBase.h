#pragma once

#include "CoreMinimal.h"
#include "ChanneldNetConnection.h"
#include "AreaOfInterestBase.generated.h"

UCLASS(Blueprintable, Abstract)
class CHANNELDUE_API UAreaOfInterestBase : public UObject
{
	GENERATED_BODY()
	
public:
	UAreaOfInterestBase() {}

	virtual void OnActivate() {}
	virtual void OnDeactivate() {}
	virtual void SetSpatialQuery(channeldpb::SpatialInterestQuery* Query, const FVector& PawnLocation, const FRotator& PawnRotation) PURE_VIRTUAL(,);
	void FollowPlayer(APlayerController* PC) {FollowingPC = PC;}
	void UnfollowPlayer() {FollowingPC = nullptr;}
	// virtual bool IsTickable() {return FollowingPC.IsValid();}
	// virtual void Tick(float DeltaTime){}

	FName Name;
	
protected:
	
	TWeakObjectPtr<APlayerController> FollowingPC;
};
