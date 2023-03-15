#pragma once

#include "CoreMinimal.h"
#include "ChanneldCookCommandlet.h"
#include "CookAndGenRepCommandlet.generated.h"


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
class REPLICATORGENERATOR_API UCookAndGenRepCommandlet : public UChanneldCookCommandlet
{
	GENERATED_BODY()

public:
	UCookAndGenRepCommandlet();

	virtual int32 Main(const FString& CmdLineParams) override;
};
