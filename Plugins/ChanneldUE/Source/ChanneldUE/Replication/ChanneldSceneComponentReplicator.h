#pragma once

#include "CoreMinimal.h"
#include "ChannelDataProvider.h"
#include "Components/SceneComponent.h"

class CHANNELDUE_API FChanneldSceneComponentReplicator
{
public:
	FChanneldSceneComponentReplicator(USceneComponent* InSceneComp, AActor* InActor);
	virtual ~FChanneldSceneComponentReplicator();

	FORCEINLINE USceneComponent* GetSceneComponent() { return SceneComp.Get(); }
	FORCEINLINE uint32 GetNetGUID() { return NetGUID; }

	FORCEINLINE bool IsStateChanged() { return bStateChanged; }
	FORCEINLINE unrealpb::SceneComponentState* GetState() { return State; }
	FORCEINLINE void ClearState();

	virtual void Tick(float DeltaTime);

protected:
	TWeakObjectPtr<USceneComponent> SceneComp;
	TWeakObjectPtr<AActor> Actor;

	uint32 NetGUID;

	bool bStateChanged;
	unrealpb::SceneComponentState* State;
	unrealpb::FVector* RelativeLocationState;
	unrealpb::FVector* RelativeRotationState;
	unrealpb::FVector* RelativeScaleState;

	static bool SetIfNotSame(unrealpb::FVector* VectorToSet, const FVector& VectorToCheck);
	static EAttachmentRule GetAttachmentRule(bool bShouldSnapWhenAttached, bool bAbsolute);
	
	// Local -> channeld
	virtual void OnTransformUpdated(USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport);

public:
	// channeld -> local
	virtual void OnStateChanged(const unrealpb::SceneComponentState* NewState);

};