#pragma once

#include "CoreMinimal.h"
#include "ChanneldReplicatorBase.h"
#include "GameFramework/PlayerState.h"
#include "unreal_common.pb.h"

// Deprecated: Missing some properties like UniqueId, PlayerName, etc. Will be replaced with the generated code.
class CHANNELDUE_API FChanneldPlayerStateReplicator : public FChanneldReplicatorBase
{
public:
	FChanneldPlayerStateReplicator(UObject* InTargetObj);
	virtual ~FChanneldPlayerStateReplicator() override;

	//~Begin FChanneldReplicatorBase Interface
	virtual UClass* GetTargetClass() override { return APlayerState::StaticClass(); }
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

	
private:
	float* ScorePtr;
	int32* PlayerIdPtr;
	uint8* PingPtr;
};