#pragma once

#include "CoreMinimal.h"
#include "ChannelDataProvider.h"
#include "GameFramework\Character.h"

class CHANNELDUE_API FChanneldCharacterReplicator
{
public:
	FChanneldCharacterReplicator(ACharacter* InCharacter);
	virtual ~FChanneldCharacterReplicator();

protected:
	TWeakObjectPtr<ACharacter> Character;
};