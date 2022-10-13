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
	virtual UClass* GetTargetClass() { return USceneComponent::StaticClass(); }
	virtual google::protobuf::Message* GetDeltaState() override { return DeltaState; }
	virtual void ClearState() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnStateChanged(const google::protobuf::Message* NewState) override;
	//~End FChanneldReplicatorBase Interface

protected:
	TWeakObjectPtr<USceneComponent> SceneComp;
	unrealpb::SceneComponentState* FullState;
	unrealpb::SceneComponentState* DeltaState;

	static EAttachmentRule GetAttachmentRule(bool bShouldSnapWhenAttached, bool bAbsolute);

private:
	// Pointers to the inaccessible Replicated properties 
	uint8* bShouldBeAttachedPtr;
	uint8* bShouldSnapLocationWhenAttachedPtr;
	uint8* bShouldSnapRotationWhenAttachedPtr;
};