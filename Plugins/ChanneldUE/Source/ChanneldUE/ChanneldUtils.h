#pragma once

#include "CoreMinimal.h"
#include "unreal_common.pb.h"
#include "Engine/PackageMapClient.h"
#include "ChanneldTypes.h"
#include "Engine/ActorChannel.h"
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

	static UObject* GetObjectByRef(const unrealpb::UnrealObjectRef* Ref, UWorld* World)
	{
		bool bUnmapped;
		return GetObjectByRef(Ref, World, bUnmapped);
	}

	static UObject* GetObjectByRef(const unrealpb::UnrealObjectRef* Ref, UWorld* World, bool& bNetGUIDUnmapped)
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
		
		bNetGUIDUnmapped = (World->GetNetDriver() == nullptr);
		if (bNetGUIDUnmapped)
		{
			return nullptr;
		}
		auto GuidCache = World->GetNetDriver()->GuidCache;
		auto Obj = GuidCache->GetObjectFromNetGUID(NetGUID, false);
		if (Obj == nullptr)
		{
			if (!GuidCache->IsNetGUIDAuthority())
			{
				UNetConnection* Connection = World->GetNetDriver()->ServerConnection;
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
					FString PathName = UTF8_TO_TCHAR(CachedObj->pathname().c_str());
					// Remap name for PIE
					GEngine->NetworkRemapPath(Connection, PathName, true);
					GuidCache->RegisterNetGUIDFromPath_Client(NewGUID, PathName, FNetworkGUID(CachedObj->outerguid()), 0, false, false);
					UObject* NewObj = GuidCache->GetObjectFromNetGUID(NewGUID, false);
					UE_LOG(LogChanneld, Verbose, TEXT("[Client] Registered NetGUID %d from path: %s"), CachedObj->netguid(), *PathName);
				}

				if (Ref->bunchbitsnum() > 0)
				{
					FInBunch InBunch(World->GetNetDriver()->ServerConnection, (uint8*)Ref->netguidbunch().data(), Ref->bunchbitsnum());
					auto PackageMap = Cast<UPackageMapClient>(World->GetNetDriver()->ServerConnection->PackageMap);

					UActorChannel* Channel = (UActorChannel*)Connection->CreateChannelByName(NAME_Actor, EChannelCreateFlags::OpenedLocally);
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
						UE_LOG(LogChanneld, Verbose, TEXT("[Client] Created new actor '%s' with NetGUID %d"), *Actor->GetName(), GuidCache->GetNetGUID(Actor).Value);
						return Actor;
					}
				}
			}

			if (Obj == nullptr)
			{
				bNetGUIDUnmapped = true;
				UE_LOG(LogChanneld, Warning, TEXT("[Client] Unable to create object from NetGUID: %d"), NetGUID.Value);
			}
		}
		return Obj;
	}

	static const unrealpb::UnrealObjectRef GetRefOfObject(UObject* Obj, UNetConnection* Connection = nullptr)
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
		auto GuidCache = World->GetNetDriver()->GuidCache;
		auto NetGUID = GuidCache->GetNetGUID(Obj);
		if (Obj->IsA<AActor>() && GuidCache->IsNetGUIDAuthority())
		{
			auto Actor = Cast<AActor>(Obj);
			if (Connection == nullptr)
			{
				Connection = Cast<UChanneldNetConnection>(Actor->GetNetConnection());
				if (Connection == nullptr)
				{
					UE_LOG(LogChanneld, Warning, TEXT("Failed to get the ref of %s: the actor has no NetConnection"), *Obj->GetName());
					return DefaultValue;
				}
			}
			auto PackageMap = CastChecked<UPackageMapClient>(Connection->PackageMap);

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
				UActorChannel* Channel = (UActorChannel*)Connection->CreateChannelByName(NAME_Actor, EChannelCreateFlags::OpenedLocally);
				if (Channel)
				{
					Channel->SetChannelActor(Actor, ESetChannelActorFlags::None);
				}
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

			}
			//else
			//{
			//	NetGUID = GuidCache->GetOrAssignNetGUID(Obj);
			//}
		}

		ObjRef.set_netguid(NetGUID.Value);
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
		const unrealpb::ActorComponentRef DefaultValue = unrealpb::ActorComponentRef::default_instance();
		if (!Comp || !Comp->GetOwner())
		{
			return DefaultValue;
		}
		auto World = Comp->GetWorld();
		if (!World)
		{
			return DefaultValue;
		}

		unrealpb::ActorComponentRef CompRef;
		CompRef.mutable_owner()->MergeFrom(GetRefOfObject(Comp->GetOwner(), Connection));
		CompRef.set_compname(std::string(TCHAR_TO_UTF8(*Comp->GetFName().ToString())));
		return CompRef;
	}
};