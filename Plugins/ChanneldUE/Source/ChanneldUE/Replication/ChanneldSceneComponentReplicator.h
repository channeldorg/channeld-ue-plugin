#pragma once

#include "CoreMinimal.h"
#include "ChannelDataProvider.h"
#include "Components/SceneComponent.h"

class AChanneldActor;

class CHANNELDUE_API FChanneldSceneComponentReplicator
{
public:
	FChanneldSceneComponentReplicator(USceneComponent* InSceneComp, AChanneldActor* InActor);
	virtual ~FChanneldSceneComponentReplicator();

	FORCEINLINE USceneComponent* GetSceneComponent() { return SceneComp.Get(); }
	FORCEINLINE uint32 GetNetGUID() { return NetGUID; }

	FORCEINLINE bool IsStateChanged() { return bStateChanged; }
	FORCEINLINE unrealpb::SceneComponentState* GetState() { return State; }
	FORCEINLINE void ClearState();

	virtual void Tick(float DeltaTime);

protected:
	TWeakObjectPtr<USceneComponent> SceneComp;
	AChanneldActor* Actor;

	uint32 NetGUID;

	bool bStateChanged;
	unrealpb::SceneComponentState* State;
	unrealpb::FVector* RelativeLocationState;
	unrealpb::FVector* RelativeRotationState;
	unrealpb::FVector* RelativeScaleState;

	static bool SetIfNotSame(unrealpb::FVector* VectorToSet, const FVector& VectorToCheck);
	
	// Local -> channeld
	virtual void OnTransformUpdated(USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport);

public:
	// channeld -> local
	virtual void OnStateChanged(const unrealpb::SceneComponentState* NewState);

};