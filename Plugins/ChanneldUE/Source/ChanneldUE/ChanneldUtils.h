#pragma once

#include "CoreMinimal.h"
#include "unreal_common.pb.h"
#include "Engine/PackageMapClient.h"

class ChanneldUtils
{
public:
	static google::protobuf::Message* CreateProtobufMessage(const std::string& FullName)
	{
		const google::protobuf::Descriptor* Desc = google::protobuf::DescriptorPool::generated_pool()
			->FindMessageTypeByName(FullName);
		if (Desc)
		{
			return google::protobuf::MessageFactory::generated_factory()->GetPrototype(Desc)->New();
		}
		return nullptr;
	}

	static FVector GetVector(const unrealpb::FVector& InVec)
	{
		return FVector(InVec.x(), InVec.y(), InVec.z());
	}

	static FRotator GetRotator(const unrealpb::FVector& InVec)
	{
		return FRotator(InVec.x(), InVec.y(), InVec.z());
	}

	// TODO
	static UObject* GetObjectByRef(const unrealpb::UnrealObjectRef* Ref, const UWorld* World)
	{
		if (!Ref || !World)
		{
			return nullptr;
		}
		return World->GetNetDriver()->GuidCache->GetObjectFromNetGUID(FNetworkGUID(Ref->netguid()), false);
	}

	static const unrealpb::UnrealObjectRef GetRefOfObject(UObject* Obj)
	{
		const unrealpb::UnrealObjectRef DefaultValue;
		if (!Obj)
		{
			return DefaultValue;
		}
		auto World = Obj->GetWorld();
		if (!World)
		{
			return DefaultValue;
		}

		auto NetGUID = World->GetNetDriver()->GuidCache->GetOrAssignNetGUID(Obj);
		unrealpb::UnrealObjectRef ObjRef;
		ObjRef.set_netguid(NetGUID.Value);
		return ObjRef;
	}
};