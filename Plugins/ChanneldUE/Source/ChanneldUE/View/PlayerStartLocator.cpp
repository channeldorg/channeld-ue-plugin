// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerStartLocator.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

UPlayerStartLocatorBase::UPlayerStartLocatorBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FVector UPlayerStartLocatorBase::GetPlayerStartPosition_Implementation(int64 ConnId, AActor*& StartSpot) const
{
	if (auto Itr = TActorIterator<APlayerStart>(GetWorld()))
	{
		StartSpot = *Itr;
		return StartSpot->GetActorLocation();
	}
	return FVector::ZeroVector;
}

UPlayerStartLocator_ModByConnId::UPlayerStartLocator_ModByConnId(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FVector UPlayerStartLocator_ModByConnId::GetPlayerStartPosition_Implementation(int64 ConnId, AActor*& StartSpot) const
{
	TArray<AActor*> PlayerStarts;
	for (auto Itr = TActorIterator<APlayerStart>(GetWorld()); Itr; ++Itr)
	{
		PlayerStarts.Add(*Itr);
	}
	if (PlayerStarts.Num() > 0)
	{
		StartSpot = PlayerStarts[ConnId % PlayerStarts.Num()];
		return StartSpot->GetActorLocation();
	}
	return FVector::ZeroVector;
}
