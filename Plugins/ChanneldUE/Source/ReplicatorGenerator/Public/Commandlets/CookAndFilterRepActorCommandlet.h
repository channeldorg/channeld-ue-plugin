// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/CookCommandlet.h"
#include "CookAndFilterRepActorCommandlet.generated.h"

/**
 * 
 */
UCLASS()
class REPLICATORGENERATOR_API UCookAndFilterRepActorCommandlet : public UCookCommandlet
{
	GENERATED_BODY()

public:
	UCookAndFilterRepActorCommandlet();

	virtual int32 Main(const FString& CmdLineParams) override;

	static void LoadResult(TArray<FString>& Result, bool& Success);

	void SaveResult(TArray<FString>);
};
