#include "ChanneldObjectReplicator.h"

#include "ChanneldUtils.h"

FChanneldObjectReplicator::FChanneldObjectReplicator(UObject* InTargetObj)
	: FChanneldReplicatorBase(InTargetObj)
{
	ObjRef = MakeShared<unrealpb::UnrealObjectRef>();
}

google::protobuf::Message* FChanneldObjectReplicator::GetDeltaState()
{
	return ObjRef.Get();
}

void FChanneldObjectReplicator::Tick(float DeltaTime)
{
	if (ObjRef->netguid() == 0)
	{
		if (TargetObject.IsValid())
		{
			ObjRef = ChanneldUtils::GetRefOfObject(TargetObject.Get(), nullptr, true);
			NetGUID = ObjRef->netguid();
			bStateChanged = true;
		}
	}
}

void FChanneldObjectReplicator::OnStateChanged(const google::protobuf::Message* NewState)
{
	if (NewState)
	{
		auto InObjRef = static_cast<const unrealpb::UnrealObjectRef*>(NewState);
		if (InObjRef->netguid() > 0 && InObjRef->netguid() != ObjRef->netguid())
		{
			ObjRef->CopyFrom(*NewState);
			NetGUID = ObjRef->netguid();
		}
	}
}
