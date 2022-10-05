#pragma once

#include "CoreMinimal.h"
#include "ChanneldReplicatorBase.h"
#include "GameFramework/PlayerState.h"

class CHANNELDUE_API FChanneldPlayerStateReplicator : public FChanneldReplicatorBase
{
public:
	FChanneldPlayerStateReplicator(UObject* InTargetObj);
	virtual ~FChanneldPlayerStateReplicator();

	//~Begin FChanneldReplicatorBase Interface
	virtual google::protobuf::Message* GetDeltaState() override;
	virtual void ClearState() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnStateChanged(const google::protobuf::Message* NewState) override;
	//~End FChanneldReplicatorBase Interface

protected:
	TWeakObjectPtr<APlayerState> PlayerState;

	// [Server+Client] The accumulated channel data of the target object
	unrealpb::PlayerState* FullState;
	// [Server] The accumulated delta change before next send
	unrealpb::PlayerState* DeltaState;

};