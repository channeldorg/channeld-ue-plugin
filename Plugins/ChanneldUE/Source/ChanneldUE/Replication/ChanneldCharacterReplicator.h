#pragma once

#include "CoreMinimal.h"
#include "ChannelDataProvider.h"
#include "GameFramework\Character.h"
#include "ChanneldReplicatorBase.h"

class CHANNELDUE_API FChanneldCharacterReplicator : public FChanneldReplicatorBase
{
public:
	FChanneldCharacterReplicator(UObject* InTargetObj);
	virtual ~FChanneldCharacterReplicator();

	//~Begin FChanneldReplicatorBase Interface
	virtual google::protobuf::Message* GetState() override;
	virtual void ClearState() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnStateChanged(const google::protobuf::Message* NewState) override;
	//~End FChanneldReplicatorBase Interface

protected:
	TWeakObjectPtr<ACharacter> Character;
	unrealpb::CharacterState* State;
	unrealpb::BaseMovementInfo* MovementInfo;
};
