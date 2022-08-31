#pragma once

#include "CoreMinimal.h"
#include "ChannelDataProvider.h"
#include "Components/SceneComponent.h"
#include "ChanneldReplicatorBase.h"

class CHANNELDUE_API FChanneldSceneComponentReplicator : public FChanneldReplicatorBase
{
public:
	FChanneldSceneComponentReplicator(USceneComponent* InSceneComp);
	virtual ~FChanneldSceneComponentReplicator();

	//~Begin FChanneldReplicatorBase Interface
	virtual google::protobuf::Message* GetState() override { return State; }
	virtual void ClearState() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnStateChanged(const google::protobuf::Message* NewState) override;
	//~End FChanneldReplicatorBase Interface

protected:
	TWeakObjectPtr<USceneComponent> SceneComp;
	unrealpb::SceneComponentState* State;
	unrealpb::FVector* RelativeLocationState;
	unrealpb::FVector* RelativeRotationState;
	unrealpb::FVector* RelativeScaleState;

	static EAttachmentRule GetAttachmentRule(bool bShouldSnapWhenAttached, bool bAbsolute);
	
	// Local -> channeld
	virtual void OnTransformUpdated(USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport);

//public:
//	// channeld -> local
//	virtual void OnStateChanged(const unrealpb::SceneComponentState* NewState);

};