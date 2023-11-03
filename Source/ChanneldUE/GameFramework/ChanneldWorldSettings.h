#pragma once

#include "CoreMinimal.h"
#include "GameFramework/WorldSettings.h"
#include "Replication/ChanneldReplicationComponent.h"
#include "ChanneldWorldSettings.generated.h"

// Responsibility:
// 1. Create the ChanneldReplication component for the WorldSettings
// 2. Route the process of any server UFunction to the the Master server
UCLASS()
class AChanneldWorldSettings : public AWorldSettings
{
	GENERATED_BODY()

public:
	AChanneldWorldSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual int32 GetFunctionCallspace(UFunction* Function, FFrame* Stack) override;

	static FName ReplicationComponentName;
	
private:
	UPROPERTY(Category=Channeld, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	UChanneldReplicationComponent* ReplicationComponent;
};