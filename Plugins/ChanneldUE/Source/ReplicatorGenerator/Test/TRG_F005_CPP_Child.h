#pragma once

#include "CoreMinimal.h"
#include "TRG_F005_CPP_Parent.h"
#include "TRG_F005_CPP_Child.generated.h"

/**
 * 
 */
UCLASS()
class REPLICATORGENERATOR_API ATRG_F005_CPP_Child : public ATRG_F005_CPP_Parent
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ATRG_F005_CPP_Child();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	uint32 UI32Property02;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
