#pragma once

#include "CoreMinimal.h"
#include "unreal_common.pb.h"
#include "Engine/PackageMapClient.h"
#include "Engine/ActorChannel.h"
#include "ChanneldConnection.h"
#include "ChanneldNetConnection.h"
#include "Engine/DemoNetDriver.h"

class CHANNELDUE_API ChanneldUtils
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

	static void SetVectorFromPB(FVector& VectorToSet, const unrealpb::FVector& VectorToCheck)
	{
		if (VectorToCheck.has_x())
		{
			VectorToSet.X = VectorToCheck.x();
		}
		if (VectorToCheck.has_y())
		{
			VectorToSet.Y = VectorToCheck.y();
		}
		if (VectorToCheck.has_z())
		{
			VectorToSet.Z = VectorToCheck.z();
		}
	}

	static void SetRotatorFromPB(FRotator& RotatorToSet, const unrealpb::FVector& RotatorToCheck)
	{
		if (RotatorToCheck.has_x())
		{
			RotatorToSet.Pitch = RotatorToCheck.x();
		}
		if (RotatorToCheck.has_y())
		{
			RotatorToSet.Yaw = RotatorToCheck.y();
		}
		if (RotatorToCheck.has_z())
		{
			RotatorToSet.Roll = RotatorToCheck.z();
		}
	}

	[[deprecated("Use SetVectorFromPB instead. Use GetVector can cause the value of x/y/z to be 0 if the it is not set.")]]
	static FVector GetVector(const unrealpb::FVector& InVec)
	{
		return FVector(InVec.x(), InVec.y(), InVec.z());
	}

	static unrealpb::FVector GetVectorPB(const FVector& InVec)
	{
		unrealpb::FVector Vec;
		Vec.set_x(InVec.X);
		Vec.set_y(InVec.Y);
		Vec.set_z(InVec.Z);
		return Vec;
	}

	[[deprecated("Use SetRotatorFromPB instead. Use GetRotator can cause the value of pitch/yaw/roll to be 0 if the it is not set.")]]
	static FRotator GetRotator(const unrealpb::FVector& InVec)
	{
		return FRotator(InVec.x(), InVec.y(), InVec.z());
	}

	static bool CheckDifference(const FVector& VectorToCheck, const unrealpb::FVector* VectorPBToCheck)
	{
		if (!FMath::IsNearlyEqual(VectorPBToCheck->x(), VectorToCheck.X))
		{
			return true;
		}
		if (!FMath::IsNearlyEqual(VectorPBToCheck->y(), VectorToCheck.Y))
		{
			return true;
		}
		if (!FMath::IsNearlyEqual(VectorPBToCheck->z(), VectorToCheck.Z))
		{
			return true;
		}
		return false;
	}

	static bool CheckDifference(const FRotator& RotatorToCheck, const unrealpb::FVector* RotatorPBToCheck)
	{
		if (!FMath::IsNearlyEqual(RotatorPBToCheck->x(), RotatorToCheck.Pitch))
		{
			return true;
		}
		if (!FMath::IsNearlyEqual(RotatorPBToCheck->y(), RotatorToCheck.Yaw))
		{
			return true;
		}
		if (!FMath::IsNearlyEqual(RotatorPBToCheck->z(), RotatorToCheck.Roll))
		{
			return true;
		}
		return false;
	}

	/**
	 * @brief Check if the VectorToCheck is different from VectorPBToCheck and set the difference to VectorToSet
	 * @param VectorToSet The Protobuf vector to set the difference to
	 * @param VectorToCheck The UE vector to check the difference from
	 * @param VectorPBToCheck The Protobuf vector to compare with the UE vector. If nullptr, VectorToSet will be used
	 * @return True if any component(x/y/z) in the VectorToCheck is different from VectorPBToCheck
	 */
	static bool SetVectorToPB(unrealpb::FVector* VectorToSet, const FVector& VectorToCheck, const unrealpb::FVector* VectorPBToCheck = nullptr)
	{
		if (VectorPBToCheck == nullptr)
			VectorPBToCheck = VectorToSet;
		
		bool bNotSame = false;
		if (!FMath::IsNearlyEqual(VectorPBToCheck->x(), VectorToCheck.X))
		{
			VectorToSet->set_x(VectorToCheck.X);
			bNotSame = true;
		}
		if (!FMath::IsNearlyEqual(VectorPBToCheck->y(), VectorToCheck.Y))
		{
			VectorToSet->set_y(VectorToCheck.Y);
			bNotSame = true;
		}
		if (!FMath::IsNearlyEqual(VectorPBToCheck->z(), VectorToCheck.Z))
		{
			VectorToSet->set_z(VectorToCheck.Z);
			bNotSame = true;
		}
		
		return bNotSame;
	}

	static bool SetRotatorToPB(unrealpb::FVector* RotatorToSet, const FRotator& RotatorToCheck, const unrealpb::FVector* RotatorPBToCheck = nullptr)
	{
		if (RotatorPBToCheck == nullptr)
			RotatorPBToCheck = RotatorToSet;
		
		bool bNotSame = false;
		if (!FMath::IsNearlyEqual(RotatorPBToCheck->x(), RotatorToCheck.Pitch))
		{
			RotatorToSet->set_x(RotatorToCheck.Pitch);
			bNotSame = true;
		}
		if (!FMath::IsNearlyEqual(RotatorPBToCheck->y(), RotatorToCheck.Yaw))
		{
			RotatorToSet->set_y(RotatorToCheck.Yaw);
			bNotSame = true;
		}
		if (!FMath::IsNearlyEqual(RotatorPBToCheck->z(), RotatorToCheck.Roll))
		{
			RotatorToSet->set_z(RotatorToCheck.Roll);
			bNotSame = true;
		}
		return bNotSame;
	}

	static channeldpb::SpatialInfo ToSpatialInfo(const FVector& Location)
	{
		channeldpb::SpatialInfo SpatialInfo;
		SpatialInfo.set_x(Location.X);
		// Swap the Y and Z as UE uses the Z-Up rule but channeld uses the Y-up rule.
		SpatialInfo.set_y(Location.Z);
		SpatialInfo.set_z(Location.Y);
		return SpatialInfo;
	}

	static void SetSpatialInfoPB(channeldpb::SpatialInfo* SpatialInfo, const FVector& Vector)
	{
		SpatialInfo->set_x(Vector.X);
		// Swap the Y and Z as UE uses the Z-Up rule but channeld uses the Y-up rule.
		SpatialInfo->set_y(Vector.Z);
		SpatialInfo->set_z(Vector.Y);
	}

	static UObject* GetObjectByRef(const unrealpb::UnrealObjectRef* Ref, UWorld* World, bool bCreateIfNotInCache = true, UChanneldNetConnection* ClientConn = nullptr)
	{
		bool bUnmapped;
		return GetObjectByRef(Ref, World, bUnmapped, bCreateIfNotInCache, ClientConn);
	}

	/**
	 * @brief Get the object in the GuidCache or create from the UnrealObjectRef.
	 * @param Ref The UnrealObjectRef message that contains the NetworkGUID and other data needed to deserialize the object.
	 * @param World Used for acquiring the NetDriver.
	 * @param bNetGUIDUnmapped Set to true if the NetDriver does not exist or the object is not in the cache.
	 * @param bCreateIfNotInCache Create the object if it's not in the cache.
	 * @param ClientConn The client connection that causes the handover, and is responsible for deserializing the object. Only need in server, when the object is handed over from another server.
	 * @return The object if it's in the GuidCache or deserialized successfully.
	 */
	static UObject* GetObjectByRef(const unrealpb::UnrealObjectRef* Ref, UWorld* World, bool& bNetGUIDUnmapped, bool bCreateIfNotInCache = true, UChanneldNetConnection* ClientConn = nullptr);
	
	static unrealpb::UnrealObjectRef GetRefOfObject(UObject* Obj, UNetConnection* Connection = nullptr);
		
	static UActorComponent* GetActorComponentByRef(const unrealpb::ActorComponentRef* Ref, UWorld* World, bool bCreateIfNotInCache = true, UChanneldNetConnection* ClientConn = nullptr);
	
	static UActorComponent* GetActorComponentByRefChecked(const unrealpb::ActorComponentRef* Ref, UWorld* World, bool& bNetGUIDUnmapped, bool bCreateIfNotInCache = true, UChanneldNetConnection* ClientConn = nullptr);

	static unrealpb::ActorComponentRef GetRefOfActorComponent(UActorComponent* Comp, UNetConnection* Connection = nullptr);

	/**
	 *	Standard method of serializing a new actor on server.
	 *		For static actors, this will just be a single call to SerializeObject, since they can be referenced by their path name.
	 *		For dynamic actors, first the actor's reference is serialized but will not resolve on clients since they haven't spawned the actor yet.
	 *		The actor archetype is then serialized along with the starting location, rotation, and velocity.
	 *		After reading this information, the client spawns this actor in the NetDriver's World and assigns it the NetGUID it read at the top of the function.
	 *
	 *		returns true if a new actor was spawned. false means an existing actor was found for the netguid.
	 */
	bool static SerializeNewActor_Server(UNetConnection* Connection, UPackageMapClient* PackageMap, TSharedPtr<FNetGUIDCache> GuidCache, FArchive& Ar, class UActorChannel *Channel, class AActor*& Actor);
	
	// Set the actor's NetRole on the client based on the NetConnection that owns the actor.
	static void SetActorRoleByOwningConnId(AActor* Actor, Channeld::ConnectionId OwningConnId);

	static ENetRole ServerGetActorNetRole(AActor* Actor);
	
	static uint32 GetNativeNetId(uint32 UniqueNetId)
	{
		return UniqueNetId & ((1 << Channeld::ConnectionIdBitOffset) - 1);
	}

	/* CDO is not available yet after the RegisterNetGUIDFromPath is called. The class of the returned UObject is always UPackage!
	// Get the context UObject from the UnrealObjectRef, which is used for checking the ClassDefaultObject.
	static UObject* GetContextObjByRef(const unrealpb::UnrealObjectRef* Ref, UWorld* World)
	{
		if (!World || !World->GetNetDriver())
		{
			return nullptr;
		}

		auto ObjLookup = World->GetNetDriver()->GuidCache->ObjectLookup;

		TArray<const unrealpb::UnrealObjectRef_GuidCachedObject*> ContextObjs;
		for (auto& Context : Ref->context())
		{
			ContextObjs.Add(&Context);
		}
		// Sort by the NetGUID to register in descending order.
		ContextObjs.Sort([](const unrealpb::UnrealObjectRef_GuidCachedObject& Obj1, const unrealpb::UnrealObjectRef_GuidCachedObject& Obj2)
			{
				return Obj1.netguid() > Obj2.netguid();
			});

		for (auto ContextObj : ContextObjs)
		{
			FNetworkGUID NetGUID = FNetworkGUID(ContextObj->netguid());
			FString PathName = UTF8_TO_TCHAR(ContextObj->pathname().c_str());
			// Remap name for PIE
			GEngine->NetworkRemapPath(World->GetNetDriver()->ServerConnection, PathName, true);
			World->GetNetDriver()->GuidCache->RegisterNetGUIDFromPath_Client(NetGUID, PathName, ContextObj->outerguid(), 0, false, true);
			if (UObject* Obj = World->GetNetDriver()->GuidCache->GetObjectFromNetGUID(NetGUID, false))
			{
				return Obj;
			}
		}

		return nullptr;
	}
	*/
};
