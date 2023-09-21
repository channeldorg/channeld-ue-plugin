#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ChanneldGameMode.generated.h"

// Responsibility: Set the right FActorSpawnParameters::Owner for SpawnActor(), so UChannelDataView::SendSpawnToClients() can send with the right owningConnId.
// REQUIRED for using the spatial channel.
UCLASS(BlueprintType)
class AChanneldGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
};