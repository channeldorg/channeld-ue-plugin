#pragma once

#include "CoreMinimal.h"
#include "ChannelDataProvider.h"
#include "Components/ActorComponent.h"
#include "ChanneldReplicatorBase.h"

class CHANNELDUE_API FChanneldActorComponentReplicator : public FChanneldReplicatorBase_AC
{
public:
	FChanneldActorComponentReplicator(UObject* InTargetObj);
	virtual ~FChanneldActorComponentReplicator() override;

	//~Begin FChanneldReplicatorBase Interface
	virtual UClass* GetTargetClass() override { return UActorComponent::StaticClass(); }
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