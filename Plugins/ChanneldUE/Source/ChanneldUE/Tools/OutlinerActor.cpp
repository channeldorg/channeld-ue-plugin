// Fill out your copyright notice in the Description page of Project Settings.


#include "Tools/OutlinerActor.h"

AOutlinerActor::AOutlinerActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
}

void AOutlinerActor::SetOutlineColor(ChannelId ChId, const FLinearColor& InColor)
{
	if (DynaMat == nullptr)
	{
		DynaMat = CreateDynamicMaterialInstance();
	}
	DynaMat->SetVectorParameterValue(ColorParamName, InColor);
	DynaMat->SetScalarParameterValue(StencilParamName, ChId % 255);

	if (Target.IsValid())
	{
		TArray<UPrimitiveComponent*> Comps;
		Target->GetComponents(Comps);
		for(UPrimitiveComponent* Comp : Comps)
		{
			Comp->SetRenderCustomDepth(true);
			Comp->SetCustomDepthStencilValue(ChId % 255);
		}
	}
}

void AOutlinerActor::SetFollowTarget(AActor* InTarget)
{
	Target = InTarget;
	/*
	FVector Origin;
	FVector Extent;
	InTarget->GetActorBounds(false, Origin, Extent);
	SetActorRelativeScale3D(Extent * 2);
	*/
}

void AOutlinerActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Target.IsValid())
	{
		SetActorLocation(Target->GetActorLocation());
	}
	else
	{
		GetWorld()->DestroyActor(this);
	}
}
