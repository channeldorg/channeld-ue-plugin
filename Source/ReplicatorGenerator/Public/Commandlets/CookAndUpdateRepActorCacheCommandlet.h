#pragma once

#include "CoreMinimal.h"
#include "ChanneldCookCommandlet.h"

#include "CookAndUpdateRepActorCacheCommandlet.generated.h"

class FLoadedObjectListener : public FUObjectArray::FUObjectCreateListener
{
public:

	void StartListen();

	void StopListen();

	virtual void NotifyUObjectCreated(const UObjectBase* Object, int32 Index) override;

	virtual void OnUObjectArrayShutdown() override;

	TSet<FString> CheckedClasses;
	TSet<FSoftClassPath> FilteredClasses;
	TArray<UObject*> CreatedObjects;
};

UCLASS()
class REPLICATORGENERATOR_API UCookAndUpdateRepActorCacheCommandlet : public UChanneldCookCommandlet
{
	GENERATED_BODY()

public:
	UCookAndUpdateRepActorCacheCommandlet();

	virtual int32 Main(const FString& CmdLineParams) override;
	
private:
	static bool IsTransient(const UObjectBase* BaseObj);
};
