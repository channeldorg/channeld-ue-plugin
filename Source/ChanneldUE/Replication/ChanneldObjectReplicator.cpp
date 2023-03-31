#include "ChanneldObjectReplicator.h"

#include "ChanneldUtils.h"

FChanneldObjectReplicator::FChanneldObjectReplicator(UObject* InTargetObj)
	: FChanneldReplicatorBase(InTargetObj)
{
}

google::protobuf::Message* FChanneldObjectReplicator::GetDeltaState()
{
	return &ObjRef;
}

void FChanneldObjectReplicator::Tick(float DeltaTime)
{
	if (ObjRef.netguid() == 0)
	{
		if (TargetObject.IsValid())
		{
			ObjRef = ChanneldUtils::GetRefOfObject(TargetObject.Get());
			bStateChanged = true;
		}
	}
}

void FChanneldObjectReplicator::OnStateChanged(const google::protobuf::Message* NewState)
{
}
