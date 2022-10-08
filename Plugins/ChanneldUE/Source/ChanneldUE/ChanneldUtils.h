#pragma once

#include "CoreMinimal.h"
#include "unreal_common.pb.h"
#include "Engine/PackageMapClient.h"
#include "ChanneldTypes.h"

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
		//return World->GetNetDriver()->GuidCache->GetObjectFromNetGUID(NetGUID, false);
		auto Obj = World->GetNetDriver()->GuidCache->GetObjectFromNetGUID(NetGUID, false);
		if (Obj == nullptr)
		{
			FString PathName;
			if (Ref->has_pathname())
			{
				PathName = UTF8_TO_TCHAR(Ref->pathname().c_str());
				World->GetNetDriver()->GuidCache->RegisterNetGUIDFromPath_Client(NetGUID, PathName, FNetworkGUID(Ref->outerguid()), 0, true, true);
				Obj = World->GetNetDriver()->GuidCache->GetObjectFromNetGUID(NetGUID, false);
			}
			if (Obj == nullptr)
			{
				UE_LOG(LogChanneld, Warning, TEXT("Unable to create object from path: %s, NetGUID: %d"), *PathName, NetGUID.Value);
			}
		}
		return Obj;
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

		unrealpb::UnrealObjectRef ObjRef;
		auto NetGUID = World->GetNetDriver()->GuidCache->GetNetGUID(Obj);
		// If the NetGUID is not created yet, assign an new one and send the path + outer GUID to the client.
		if (!NetGUID.IsValid())
		{
			//NetGUID = World->GetNetDriver()->GuidCache->AssignNewNetGUID_Server(Obj);
			NetGUID = World->GetNetDriver()->GuidCache->AssignNewNetGUIDFromPath_Server(Obj->GetPathName(), Obj->GetOuter(), Obj->GetClass());
			ObjRef.set_pathname(std::string(TCHAR_TO_UTF8(*Obj->GetPathName())));
			ObjRef.set_outerguid(World->GetNetDriver()->GuidCache->GetNetGUID(Obj->GetOuter()).Value);
		}

		/*
		auto NetGUID = World->GetNetDriver()->GuidCache->GetOrAssignNetGUID(Obj);
		*/
		ObjRef.set_netguid(NetGUID.Value);
		return ObjRef;
	}
};