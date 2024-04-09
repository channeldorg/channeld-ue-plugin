#pragma once

#include "CoreMinimal.h"
#include "unreal_common.pb.h"
#include "Engine/PackageMapClient.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Engine/ActorChannel.h"
#include "ChanneldConnection.h"
#include "ChanneldNetConnection.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/DemoNetDriver.h"
//#define CHANNELD_TOLERANCE (1.e-8f)
#define CHANNELD_TOLERANCE (0.001f)

class CHANNELDUE_API ChanneldUtils
{
public:
	static bool IsServer(const UWorld* World)
	{
		if (World == nullptr)
		{
			return false;
		}
#if ENGINE_MAJOR_VERSION >= 5
		return World->GetNetMode() == NM_DedicatedServer || World->GetNetMode() == NM_ListenServer;
#else
		return World->IsServer();
#endif
	}
	
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

	[[deprecated(
		"Use SetRotatorFromPB instead. Use GetRotator can cause the value of pitch/yaw/roll to be 0 if the it is not set."
	)]]
	static FRotator GetRotator(const unrealpb::FVector& InVec)
	{
		return FRotator(InVec.x(), InVec.y(), InVec.z());
	}

	static bool CheckDifference(const FVector& VectorToCheck, const unrealpb::FVector* VectorPBToCheck)
	{
		if (!FMath::IsNearlyEqual(VectorPBToCheck->x(), VectorToCheck.X, CHANNELD_TOLERANCE))
		{
			return true;
		}
		if (!FMath::IsNearlyEqual(VectorPBToCheck->y(), VectorToCheck.Y, CHANNELD_TOLERANCE))
		{
			return true;
		}
		if (!FMath::IsNearlyEqual(VectorPBToCheck->z(), VectorToCheck.Z, CHANNELD_TOLERANCE))
		{
			return true;
		}
		
		return false;
	}

	static bool CheckDifference(const FRotator& RotatorToCheck, const unrealpb::FVector* RotatorPBToCheck)
	{
		if (!FMath::IsNearlyEqual(RotatorPBToCheck->x(), RotatorToCheck.Pitch, CHANNELD_TOLERANCE))
		{
			return true;
		}
		if (!FMath::IsNearlyEqual(RotatorPBToCheck->y(), RotatorToCheck.Yaw, CHANNELD_TOLERANCE))
		{
			return true;
		}
		if (!FMath::IsNearlyEqual(RotatorPBToCheck->z(), RotatorToCheck.Roll, CHANNELD_TOLERANCE))
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
		if (!FMath::IsNearlyEqual(VectorPBToCheck->x(), VectorToCheck.X, CHANNELD_TOLERANCE))
		{
			VectorToSet->set_x(VectorToCheck.X);
			bNotSame = true;
		}
		if (!FMath::IsNearlyEqual(VectorPBToCheck->y(), VectorToCheck.Y, CHANNELD_TOLERANCE))
		{
			VectorToSet->set_y(VectorToCheck.Y);
			bNotSame = true;
		}
		if (!FMath::IsNearlyEqual(VectorPBToCheck->z(), VectorToCheck.Z, CHANNELD_TOLERANCE))
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
		if (!FMath::IsNearlyEqual(RotatorPBToCheck->x(), RotatorToCheck.Pitch, CHANNELD_TOLERANCE))
		{
			RotatorToSet->set_x(RotatorToCheck.Pitch);
			bNotSame = true;
		}
		if (!FMath::IsNearlyEqual(RotatorPBToCheck->y(), RotatorToCheck.Yaw, CHANNELD_TOLERANCE))
		{
			RotatorToSet->set_y(RotatorToCheck.Yaw);
			bNotSame = true;
		}
		if (!FMath::IsNearlyEqual(RotatorPBToCheck->z(), RotatorToCheck.Roll, CHANNELD_TOLERANCE))
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

	static FNetworkGUID GetNetId(UObject* Obj, bool bAssignOnServer = true)
	{
		FNetworkGUID NetId;
		if (!Obj)
		{
			return NetId;
		}
		auto World = Obj->GetWorld();
		if (!World)
		{
			return NetId;
		}

		if (const auto NetDriver = World->GetNetDriver())
		{
			if (NetDriver->IsServer() && bAssignOnServer)
			{
				NetId = NetDriver->GuidCache->GetOrAssignNetGUID(Obj);
			}
			else
			{
				NetId = NetDriver->GuidCache->GetNetGUID(Obj);
			}
		}
		
		return NetId;
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
	
	static TSharedRef<unrealpb::UnrealObjectRef> GetRefOfObject(UObject* Obj, UNetConnection* Connection = nullptr, bool bFullExport = false, UWorld* World = nullptr);

	static bool CheckObjectWithRef(UObject* Obj, const unrealpb::UnrealObjectRef* Ref, UWorld* World)
	{
		bool bUnmapped = false;
		return GetObjectByRef(Ref, World, bUnmapped, false) == Obj && !bUnmapped;
	}

	static bool SetObjectPtrByRef(const unrealpb::UnrealObjectRef* Ref, UWorld* World, UObject** ObjPtr)
	{
		bool bUnmapped = false;
		const auto NewValue = ChanneldUtils::GetObjectByRef(Ref, World, bUnmapped, false);
		if (!bUnmapped && *ObjPtr != NewValue)
		{
			*ObjPtr = NewValue;
			return true;
		}
		return false;
	}
		
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

	static unrealpb::AssetRef GetAssetRef(const UObject* Obj)
	{
		unrealpb::AssetRef Ref;
		if (Obj)
		{
			Ref.set_objectpath(std::string(TCHAR_TO_UTF8(*Obj->GetPathName())));
		}
		return Ref;
	}
	
	static UObject* GetAssetByRef(const unrealpb::AssetRef* Ref)
	{
		if (Ref->objectpath().size() > 0)
		{
			const FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			const FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(UTF8_TO_TCHAR(Ref->objectpath().c_str()));
			if (AssetData.IsValid())
			{
				return AssetData.GetAsset();
			}
		}
		return nullptr;
	}
	
	// Set the actor's NetRole on the client based on the NetConnection that owns the actor.
	static void SetActorRoleByOwningConnId(AActor* Actor, Channeld::ConnectionId OwningConnId);

	// Should the owner of the actor, or the PlayerController or PlayerState of the pawn should be set?
	static bool ShouldSetPlayerControllerOrPlayerStateForActor(AActor* Actor)
	{
		const bool bIsServer = ChanneldUtils::IsServer(Actor->GetWorld());
		// Special case: the client won't create other player's controller. Pawn and PlayerState's owner is PlayerController.
		const bool bOwnerIsPC = Actor->IsA<APawn>() || Actor->IsA<APlayerState>();
		const bool bClientShouldSetOwner = !bOwnerIsPC || Actor->GetLocalRole() > ROLE_SimulatedProxy;
		return bIsServer || bClientShouldSetOwner;
	}

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

	static void InitNetConnForSpawn(UChanneldNetConnection* InNetConn)
	{
		NetConnForSpawn = InNetConn;
	}
	
	static UChanneldNetConnection* GetNetConnForSpawn()
	{
		return NetConnForSpawn;
	}

	static UNetConnection* GetActorNetConnection(const AActor* Actor);
	static void ResetNetConnForSpawn();

	// static void MarkArUseCustomSerializeObject(FArchive& Ar);
	// static bool CheckArUseCustomSerializeObject(const FArchive& Ar);
	
	static bool LoadStaticObjectExportedNetGUIDFromFile(const FString& FilePath);
	static uint32 GetStaticObjectExportedNetGUID(const FString& PathName);
	static FString GetStaticObjectExportedPathName(uint32 NetGUID);
	static void RegisterStaticObjectNetGUID_Authority(UObject* Obj, uint32 ExportID);
	static void RegisterStaticObjectNetGUID_NonAuthority(const UObject* Obj, uint32 ExportID, UNetConnection* Connection, bool bRunningOnServer = false);
	static UObject* TryLoadStaticObject(uint32 NetGUID, UNetConnection* Connection, bool bRunningOnServer = false);

private:
	static TMap<uint32, TSharedRef<unrealpb::UnrealObjectRef>> ObjRefCache;
	static UChanneldNetConnection* NetConnForSpawn;
};
