// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRG_F005_CPP_Parent.generated.h"

UCLASS()
class REPLICATORGENERATOR_API ATRG_F005_CPP_Parent : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ATRG_F005_CPP_Parent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	uint32 UI32Property01;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite)
	class UChanneldReplicationComponent* RepComponent; 
	
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
