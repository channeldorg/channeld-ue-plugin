#pragma once

#include "CoreMinimal.h"
#include "unreal_common.pb.h"
#include "Engine/PackageMapClient.h"
#include "Engine/ActorChannel.h"
#include "ChanneldTypes.h"
#include "ChanneldConnection.h"
#include "ChanneldNetConnection.h"

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
	static UObject* GetObjectByRef(const unrealpb::UnrealObjectRef* Ref, UWorld* World, bool& bNetGUIDUnmapped, bool bCreateIfNotInCache = true, UChanneldNetConnection* ClientConn = nullptr)
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

		/*
		// Client uses different NetworkGUID than server
		if (World->IsClient())
		{
			GetUniqueNetId(NetGUID, Ref->connid());
		}
		*/
		
		bNetGUIDUnmapped = (World->GetNetDriver() == nullptr);
		if (bNetGUIDUnmapped)
		{
			return nullptr;
		}
		auto GuidCache = World->GetNetDriver()->GuidCache;
		auto Obj = GuidCache->GetObjectFromNetGUID(NetGUID, false);
		if (Obj == nullptr && bCreateIfNotInCache)
		{
			if (!GuidCache->IsNetGUIDAuthority() || ClientConn)
			{
				UNetConnection* Connection = ClientConn ? ClientConn : World->GetNetDriver()->ServerConnection;
				TArray<const unrealpb::UnrealObjectRef_GuidCachedObject*> CachedObjs;
				for (auto& Context : Ref->context())
				{
					CachedObjs.Add(&Context);
				}
				// Sort by the NetGUID to register in descending order.
				CachedObjs.Sort([](const unrealpb::UnrealObjectRef_GuidCachedObject& Obj1, const unrealpb::UnrealObjectRef_GuidCachedObject& Obj2)
					{
						return Obj1.netguid() > Obj2.netguid();
					});
				for (auto CachedObj : CachedObjs)
				{
					FNetworkGUID NewGUID = FNetworkGUID(CachedObj->netguid());

					/*
					// Client uses different NetworkGUID than server
					if (World->IsClient())
					{
						GetUniqueNetId(NewGUID, Ref->connid());
					}
					*/
				
					FString PathName = UTF8_TO_TCHAR(CachedObj->pathname().c_str());
					// Remap name for PIE
					GEngine->NetworkRemapPath(Connection, PathName, true);
					if (ClientConn)
					{
						GuidCache->RegisterNetGUIDFromPath_Server(NewGUID, PathName, CachedObj->outerguid(), 0, false, false);
					}
					else
					{
						GuidCache->RegisterNetGUIDFromPath_Client(NewGUID, PathName, CachedObj->outerguid(), 0, false, false);
					}
					UObject* NewObj = GuidCache->GetObjectFromNetGUID(NewGUID, false);
					UE_LOG(LogChanneld, Verbose, TEXT("[Client] Registered NetGUID %d (%d) from path: %s"), NewGUID.Value, ChanneldUtils::GetNativeNetId(NewGUID.Value), *PathName);
				}

				if (Ref->bunchbitsnum() > 0)
				{
					FInBunch InBunch(Connection, (uint8*)Ref->netguidbunch().data(), Ref->bunchbitsnum());
					auto PackageMap = CastChecked<UPackageMapClient>(Connection->PackageMap);

					UActorChannel* Channel = (UActorChannel*)Connection->CreateChannelByName(NAME_Actor, EChannelCreateFlags::None);
					UE_LOG(LogChanneld, VeryVerbose, TEXT("[Client] ActorChannels: %d"), Connection->ActorChannelsNum());
					AActor* Actor;
					//-----------------------------------------
					// Copied from UActorChannel::ProcessBunch
					//-----------------------------------------
					if (PackageMap->SerializeNewActor(InBunch, Channel, Actor))
					{
						Channel->SetChannelActor(Actor, ESetChannelActorFlags::SkipReplicatorCreation);
						// Setup NetDriver, etc.
						Channel->NotifyActorChannelOpen(Actor, InBunch);
						// After all properties have been initialized, call PostNetInit. This should call BeginPlay() so initialization can be done with proper starting values.
						Actor->PostNetInit();
						UE_LOG(LogChanneld, Verbose, TEXT("[Client] Created new actor '%s' with NetGUID %d (%d)"), *Actor->GetName(), GuidCache->GetNetGUID(Actor).Value, ChanneldUtils::GetNativeNetId(Ref->netguid()));

						//// Remove the channel after using it
						//Channel->ConditionalCleanUp(true, EChannelCloseReason::Destroyed);
						return Actor;
					}
				}
			}

			if (Obj == nullptr)
			{
				bNetGUIDUnmapped = true;
				UE_LOG(LogChanneld, Warning, TEXT("[Client] Unable to create object from NetGUID: %d (%d)"), NetGUID.Value, ChanneldUtils::GetNativeNetId(Ref->netguid()));
			}
		}
		return Obj;
	}
	
	static const unrealpb::UnrealObjectRef GetRefOfObject(UObject* Obj, UNetConnection* Connection = nullptr)
	{
		unrealpb::UnrealObjectRef ObjRef;
		ObjRef.set_netguid(0);
		ObjRef.set_bunchbitsnum(0);

		if (!Obj)
		{
			return ObjRef;
		}
		auto World = Obj->GetWorld();
		if (!World)
		{
			return ObjRef;
		}

		auto GuidCache = World->GetNetDriver()->GuidCache;
		auto NetGUID = GuidCache->GetNetGUID(Obj);
		if (Obj->IsA<AActor>() && GuidCache->IsNetGUIDAuthority())
		{
			auto Actor = Cast<AActor>(Obj);
			if (Connection == nullptr)
			{
				Connection = Cast<UChanneldNetConnection>(Actor->GetNetConnection());
			}
			if (!IsValid(Connection))
			{
				UE_LOG(LogChanneld, Warning, TEXT("Failed to get the ref of %s: the NetConnection is not valid"), *Obj->GetName());
				return ObjRef;
			}
			auto PackageMap = Cast<UPackageMapClient>(Connection->PackageMap);

			if (IsValid(PackageMap))
			{
				// If the NetGUID is not created yet, assign an new one and send the new NetGUID CachedObjects to the client.
				// If the NetGUID is already created but hasn't been exported to the client yet, we need to send the CachedObjects as well.
				if (!NetGUID.IsValid() || !PackageMap->NetGUIDExportCountMap.Contains(NetGUID))
				{
					TSet<FNetworkGUID> OldGUIDs;
					PackageMap->NetGUIDExportCountMap.GetKeys(OldGUIDs);

					//--------------------------------------------------
					// Copied from UActorChannel::ReplicateActor (L3121)
					//--------------------------------------------------
					FOutBunch Ar(PackageMap);
					Ar.bReliable = true;
					UActorChannel* Channel = (UActorChannel*)Connection->CreateChannelByName(NAME_Actor, EChannelCreateFlags::None);
					UE_LOG(LogChanneld, VeryVerbose, TEXT("[Server] ActorChannels: %d"), Connection->ActorChannelsNum());
					Channel->SetChannelActor(Actor, ESetChannelActorFlags::None);
					PackageMap->SerializeNewActor(Ar, Channel, Actor);
					Actor->OnSerializeNewActor(Ar);
					//--------------------------------------------------

					NetGUID = GuidCache->GetNetGUID(Obj);

					TSet<FNetworkGUID> NewGUIDs;
					PackageMap->NetGUIDExportCountMap.GetKeys(NewGUIDs);
					// Find the newly-registered NetGUIDs during SerializeNewActor()
					NewGUIDs = NewGUIDs.Difference(OldGUIDs);

					for (FNetworkGUID& NewGUID : NewGUIDs)
					{
						// Don't send the target NetGUID in the context
						if (NewGUID == NetGUID)
							continue;

						auto NewCachedObj = GuidCache->GetCacheObject(NewGUID);
						auto Context = ObjRef.add_context();
						Context->set_netguid(NewGUID.Value);
						Context->set_pathname(std::string(TCHAR_TO_UTF8(*NewCachedObj->PathName.ToString())));
						Context->set_outerguid(NewCachedObj->OuterGUID.Value);
						UE_LOG(LogChanneld, Verbose, TEXT("[Server] Send registered NetGUID %d with path: %s"), NewGUID.Value, *NewCachedObj->PathName.ToString());
					}
					ObjRef.set_netguidbunch(Ar.GetData(), Ar.GetNumBytes());
					ObjRef.set_bunchbitsnum(Ar.GetNumBits());

					// Remove the channel after using it
					Channel->ConditionalCleanUp(true, EChannelCloseReason::Destroyed);
				}
				//else
				//{
				//	NetGUID = GuidCache->GetOrAssignNetGUID(Obj);
				//}
			}
			else
			{
				UE_LOG(LogChanneld, Warning, TEXT("Failed to get the ref of %s: the Actor's NetConnection has no PackageMapClient"), *Obj->GetName());
			}
		}

		ObjRef.set_netguid(NetGUID.Value);
		// ObjRef.set_connid(GEngine->GetEngineSubsystem<UChanneldConnection>()->GetConnId());
		return ObjRef;
	}

	template<class T>
	static T* GetActorComponentByRef(const unrealpb::ActorComponentRef* Ref, UWorld* World)
	{
		if (!Ref || !World)
		{
			return nullptr;
		}

		AActor* Actor = Cast<AActor>(GetObjectByRef(&Ref->owner(), World));
		if (!Actor)
		{
			return nullptr;
		}

		FName CompName = FName(UTF8_TO_TCHAR(Ref->compname().c_str()));
		UObject* Comp = Actor->GetDefaultSubobjectByName(CompName);
		if (Comp)
		{
			return Cast<T>(Comp);
		}

		UE_LOG(LogChanneld, Warning, TEXT("Cannot find component '%s' of actor %s"), *CompName.ToString(), *Actor->GetName());
		return nullptr;
	}

	static const unrealpb::ActorComponentRef GetRefOfActorComponent(UActorComponent* Comp, UNetConnection* Connection = nullptr)
	{
		unrealpb::ActorComponentRef CompRef;
		if (!Comp || !Comp->GetOwner())
		{
			CompRef.mutable_owner()->set_netguid(0);
			return CompRef;
		}
		auto World = Comp->GetWorld();
		if (!World)
		{
			CompRef.mutable_owner()->set_netguid(0);
			return CompRef;
		}

		CompRef.mutable_owner()->MergeFrom(GetRefOfObject(Comp->GetOwner(), Connection));
		CompRef.set_compname(std::string(TCHAR_TO_UTF8(*Comp->GetFName().ToString())));
		return CompRef;
	}

	/* Changed to UChanneldNetDriver::OnChanneldAuthenticated: set GuidCache->UniqueNetIDs
	// Client uses ConnId for at the first bits to resolve the conflicts of NetGUIDs on different servers.
	static FNetworkGUID GetUniqueNetId(uint32 NetId, uint32 ConnId)
	{
		FNetworkGUID NetworkGUID(NetId);
		return GetUniqueNetId(NetworkGUID, ConnId);
	}

	// Client uses ConnId for at the first bits to resolve the conflicts of NetGUIDs on different servers.
	static FNetworkGUID& GetUniqueNetId(FNetworkGUID& NetworkGUID, uint32 ConnId)
	{
		if (NetworkGUID.IsValid())
		{
			NetworkGUID.Value |= ConnId << ConnectionIdBitOffset;
		}
		return NetworkGUID;
	}
	*/

	static uint32 GetNativeNetId(uint32 UniqueNetId)
	{
		return UniqueNetId & ((1 << ConnectionIdBitOffset) - 1);
	}

	// Set the actor's NetRole on the client based on the NetConnection that owns the actor.
	static void SetActorRoleByOwningConnId(AActor* Actor, ConnectionId OwningConnId)
	{
		UChanneldConnection* ConnToChanneld = GEngine->GetEngineSubsystem<UChanneldConnection>();
		ENetRole OldRole = Actor->GetLocalRole();
		if (ConnToChanneld->GetConnId() == OwningConnId)
		{
			Actor->SetRole(ROLE_AutonomousProxy);
		}
		else if (Actor->GetLocalRole() == ROLE_AutonomousProxy)
		{
			Actor->SetRole(ROLE_SimulatedProxy);
		}
		const static UEnum* Enum = StaticEnum<ENetRole>();
		UE_LOG(LogChanneld, Verbose, TEXT("[Client] Updated actor %s's role from %s to %s, local/remote owning connId: %d/%d"),
			*Actor->GetName(),
			*Enum->GetNameStringByValue(OldRole),
			*Enum->GetNameStringByValue(Actor->GetLocalRole()),
			ConnToChanneld->GetConnId(),
			OwningConnId
		);
	}
};