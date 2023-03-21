// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChanneldCookCommandlet.h"

#include "CookAndUpdateRepRegistryCommandlet.generated.h"

class FLoadedObjectListener : public FUObjectArray::FUObjectCreateListener
{
public:

	void StartListen();

	void StopListen();

	virtual void NotifyUObjectCreated(const UObjectBase* Object, int32 Index) override;

	virtual void OnUObjectArrayShutdown() override;

	TSet<FString> CheckedClasses;
	TSet<FSoftClassPath> FilteredClasses;
};

UCLASS()
class REPLICATORGENERATOR_API UCookAndUpdateRepRegistryCommandlet : public UChanneldCookCommandlet
{
	GENERATED_BODY()

public:
	UCookAndUpdateRepRegistryCommandlet();

	virtual int32 Main(const FString& CmdLineParams) override;
};
