#pragma once

#include "CoreMinimal.h"
#include "ChannelDataProvider.h"
#include "GameFramework\Pawn.h"
#include "GameFramework/PlayerState.h"
#include "ChanneldReplicatorBase.h"

class CHANNELDUE_API FChanneldPawnReplicator : public FChanneldReplicatorBase
{
public:
	FChanneldPawnReplicator(UObject* InTargetObj);
	virtual ~FChanneldPawnReplicator() override;

	//~Begin FChanneldReplicatorBase Interface
	virtual UClass* GetTargetClass() override { return APawn::StaticClass(); }
	virtual google::protobuf::Message* GetDeltaState() override;
	virtual void ClearState() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnStateChanged(const google::protobuf::Message* NewState) override;
	//~End FChanneldReplicatorBase Interface

protected:
	TWeakObjectPtr<APawn> Pawn;

	// [Server+Client] The accumulated channel data of the target object
	unrealpb::PawnState* FullState;
	// [Server] The accumulated delta change before next send
	unrealpb::PawnState* DeltaState;

private:
	APlayerState** PlayerStatePtr;
};