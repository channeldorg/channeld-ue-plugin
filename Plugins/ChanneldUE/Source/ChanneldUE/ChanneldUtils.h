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

	static bool SetIfNotSame(unrealpb::FVector* VectorToSet, const FVector& VectorToCheck)
	{
		bool bNotSame = false;
		if (!FMath::IsNearlyEqual(VectorToSet->x(), VectorToCheck.X))
		{
			VectorToSet->set_x(VectorToCheck.X);
			bNotSame = true;
		}
		if (!FMath::IsNearlyEqual(VectorToSet->y(), VectorToCheck.Y))
		{
			VectorToSet->set_y(VectorToCheck.Y);
			bNotSame = true;
		}
		if (!FMath::IsNearlyEqual(VectorToSet->z(), VectorToCheck.Z))
		{
			VectorToSet->set_z(VectorToCheck.Z);
			bNotSame = true;
		}
		return bNotSame;
	}

	static UObject* GetObjectByRef(const unrealpb::UnrealObjectRef* Ref, const UWorld* World)
	{
		if (!Ref || !World)
		{
			return nullptr;
		}
		FNetworkGUID NetGUID(Ref->netguid());
		if (!NetGUID.IsValid())
		{
			return nullptr;
		}
		return World->GetNetDriver()->GuidCache->GetObjectFromNetGUID(NetGUID, false);
	}

	static const unrealpb::UnrealObjectRef GetRefOfObject(UObject* Obj)
	{
		const unrealpb::UnrealObjectRef DefaultValue = unrealpb::UnrealObjectRef::default_instance();
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