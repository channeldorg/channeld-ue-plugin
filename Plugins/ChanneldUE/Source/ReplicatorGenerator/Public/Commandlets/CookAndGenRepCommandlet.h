// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChanneldCookCommandlet.h"
#include "CookAndGenRepCommandlet.generated.h"

/**
 * 
 */
UCLASS()
class REPLICATORGENERATOR_API UCookAndGenRepCommandlet : public UChanneldCookCommandlet
{
	GENERATED_BODY()
public:
	UCookAndGenRepCommandlet();

	virtual int32 Main(const FString& CmdLineParams) override;
};
