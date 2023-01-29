// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/CookCommandlet.h"
#include "CookAndGenRepCommandlet.generated.h"

/**
 * 
 */
UCLASS()
class REPLICATORGENERATOR_API UCookAndGenRepCommandlet : public UCookCommandlet
{
	GENERATED_BODY()
public:
	UCookAndGenRepCommandlet();

	virtual int32 Main(const FString& CmdLineParams) override;
};
