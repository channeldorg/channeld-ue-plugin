#include "ChanneldReplicatorBase.h"
#include "ChanneldReplicationComponent.h"
#include "Engine/PackageMapClient.h"
#include "Engine/NetDriver.h"

FChanneldReplicatorBase::FChanneldReplicatorBase(UObject* InTargetObj)
{
    TargetObject = InTargetObj;

    //UWorld* World = TargetObject->GetWorld();
    //if (World && World->GetNetDriver())
    //{
    //    NetGUID = World->GetNetDriver()->GuidCache->GetOrAssignNetGUID(InTargetObj).Value;
    //}
    //else
    //{
    //    UE_LOG(LogChanneld, Warning, TEXT("Target object is not in world: %s"), *TargetObject->GetFullName());
    //}

	bStateChanged = false;
}

uint32 FChanneldReplicatorBase::GetNetGUID()
{
	if (!NetGUID.IsValid())
	{
        if (TargetObject.IsValid())
        {
		    UWorld* World = TargetObject->GetWorld();
            if (World && World->GetNetDriver())
            {
		        NetGUID = World->GetNetDriver()->GuidCache->GetNetGUID(TargetObject.Get());
            }
        }
	}
	return NetGUID.Value;
}

uint32 FChanneldReplicatorBase_AC::GetNetGUID()
{
	if (!NetGUID.IsValid())
	{
		if (TargetObject.IsValid())
		{
			UWorld* World = TargetObject->GetWorld();
			if (World && World->GetNetDriver())
			{
				NetGUID = World->GetNetDriver()->GuidCache->GetNetGUID(CastChecked<UActorComponent>(TargetObject.Get())->GetOwner());
			}
		}
	}
	return NetGUID.Value;
}
