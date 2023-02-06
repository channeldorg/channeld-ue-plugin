// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChanneldTypes.h"
#include "Engine/DecalActor.h"
#include "OutlinerActor.generated.h"

/**
 * 
 */
UCLASS()
class CHANNELDUE_API AOutlinerActor : public ADecalActor
{
	GENERATED_BODY()

public:
	AOutlinerActor(const FObjectInitializer& ObjectInitializer);
	
	void SetOutlineColor(Channeld::ChannelId ChId, const FLinearColor& InColor);
	void SetFollowTarget(AActor* InTarget);

	void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere)
	FName ColorParamName = FName("SobelColour");
	
	UPROPERTY(EditAnywhere)
	FName StencilParamName = FName("Stencil1");

private:
	UPROPERTY()
	UMaterialInstanceDynamic* DynaMat;
	
	TWeakObjectPtr<AActor> Target;
};
