#include "ChanneldReplicatorBase.h"
#include "ChanneldReplicationComponent.h"
#include "Engine/PackageMapClient.h"
#include "Engine/NetDriver.h"

FChanneldReplicatorBase::FChanneldReplicatorBase(UObject* InTargetObj)
{
    TargetObject = InTargetObj;

    UWorld* World = TargetObject->GetWorld();
    if (World)
    {
        NetGUID = World->GetNetDriver()->GuidCache->GetOrAssignNetGUID(InTargetObj).Value;
    }
    else
    {
        UE_LOG(LogChanneld, Warning, TEXT("Target object is not in world: %s"), *TargetObject->GetFullName());
    }

	bStateChanged = false;
}
