#pragma once

#include "CoreMinimal.h"
#include "ChannelDataProvider.h"
#include "Components/ActorComponent.h"
#include "ChanneldReplicatorBase.h"

class CHANNELDUE_API FChanneldActorComponentReplicator : public FChanneldReplicatorBase
{
public:
	FChanneldActorComponentReplicator(UObject* InTargetObj);
	virtual ~FChanneldActorComponentReplicator();

	//~Begin FChanneldReplicatorBase Interface
	virtual UClass* GetTargetClass() { return UActorComponent::StaticClass(); }
	virtual google::protobuf::Message* GetDeltaState();
	virtual void ClearState() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnStateChanged(const google::protobuf::Message* NewState) override;
	//~End FChanneldReplicatorBase Interface

protected:
	TWeakObjectPtr<UActorComponent> ActorComponent;
	unrealpb::ActorComponentState* FullState;
	unrealpb::ActorComponentState* DeltaState;
};