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

	static bool SetIfNotSame(unrealpb::FVector* RotatorToSet, const FRotator& RotatorToCheck)
	{
		bool bNotSame = false;
		if (!FMath::IsNearlyEqual(RotatorToSet->x(), RotatorToCheck.Pitch))
		{
			RotatorToSet->set_x(RotatorToCheck.Pitch);
			bNotSame = true;
		}
		if (!FMath::IsNearlyEqual(RotatorToSet->y(), RotatorToCheck.Yaw))
		{
			RotatorToSet->set_y(RotatorToCheck.Yaw);
			bNotSame = true;
		}
		if (!FMath::IsNearlyEqual(RotatorToSet->z(), RotatorToCheck.Roll))
		{
			RotatorToSet->set_z(RotatorToCheck.Roll);
			bNotSame = true;
		}
		return bNotSame;
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
	
	static const unrealpb::UnrealObjectRef GetRefOfObject(UObject* Obj, UNetConnection* Connection = nullptr);
		
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
	static void SetActorRoleByOwningConnId(AActor* Actor, ConnectionId OwningConnId);

	static uint32 GetNativeNetId(uint32 UniqueNetId)
	{
		return UniqueNetId & ((1 << ConnectionIdBitOffset) - 1);
	}
};
