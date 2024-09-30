#include "ChanneldUtils.h"

#include "ChanneldGameInstanceSubsystem.h"
#include "ChanneldNetDriver.h"
#include "ChanneldTypes.h"

TMap<uint32, TSharedRef<unrealpb::UnrealObjectRef>> ChanneldUtils::ObjRefCache;
UChanneldNetConnection* ChanneldUtils::NetConnForSpawn;

UObject* ChanneldUtils::GetObjectByRef(const unrealpb::UnrealObjectRef* Ref, UWorld* World, bool& bNetGUIDUnmapped, bool bCreateIfNotInCache, UChanneldNetConnection* ClientConn)
{
	if (!Ref || !World)
	{
		return nullptr;
	}
	FNetworkGUID NetGUID(Ref->netguid());
	if (!NetGUID.IsValid())
	{
		// CDO: treat it as an asset object
		if (Ref->has_classpath())
		{
			auto ObjectPath = UTF8_TO_TCHAR(Ref->classpath().c_str());
			UObject* Asset = FindObject<UObject>(nullptr, ObjectPath);
			if ( Asset == nullptr)
			{
				Asset = LoadObject<UObject>(nullptr, ObjectPath);
			}
			return Asset;
		}
		
		return nullptr;
	}

	UNetDriver* NetDriver = World->GetNetDriver();
	bNetGUIDUnmapped = (NetDriver == nullptr);
	if (bNetGUIDUnmapped)
	{
		UE_LOG(LogChanneld, Warning, TEXT("ChanneldUtils::GetObjectByRef: Unable to get the NetDriver, NetGUID: %d (%d)"), NetGUID.Value, ChanneldUtils::GetNativeNetId(Ref->netguid()));
		return nullptr;
	}
	
	auto GuidCache = NetDriver->GuidCache;
	UObject* Obj = nullptr;
	// Static object - load it directly
	if (NetGUID.IsStatic())
	{
		// Static object could be invalid (deleted when leaving AOI).
		// With classpath set, it can be re-created.
		Obj = FStaticGuidRegistry::GetStaticObject(NetGUID, NetDriver);
	}
	else
	{
		Obj = GuidCache->GetObjectFromNetGUID(NetGUID, false);
	}
	if (!IsValid(Obj) && bCreateIfNotInCache)
	{
		if (!GuidCache->IsNetGUIDAuthority() || ClientConn)
		{
			UNetConnection* Connection = ClientConn ? ClientConn : NetDriver->ServerConnection;

			/* v0.7.3 - static objects are pre-registered on both server and client, so they won't be received here.
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
				if (NewGUID.IsStatic() && NewGUID.Value >= GenManager_ChanneldStaticObjectExportStartID)
				{
					if (UObject* NewObj = FStaticGuidRegistry::TryLoadStaticObject(NewGUID.Value, Connection, NetDriver->IsServer()))
					{
						if (NewGUID == NetGUID)
						{
							Obj = NewObj;
							UE_LOG(LogChanneld, Verbose, TEXT("[Client] Registered NetGUID (%llu) Obj: %s"), NewGUID.Value, Obj ? *Obj->GetName() : TEXT("Null"));
						}
					}
					else
					{
						UE_LOG(LogChanneld, Warning, TEXT("ChanneldUtils::GetObjectByRef: Failed to load static object with NetGUID: %d (%d)"), NewGUID.Value, ChanneldUtils::GetNativeNetId(CachedObj->netguid()));
					}
					continue;
				}
				
				FString PathName = UTF8_TO_TCHAR(CachedObj->pathname().c_str());
				// Remap name for PIE
				GEngine->NetworkRemapPath(Connection, PathName, true);
				if (ClientConn)
				{
					GuidCache->RegisterNetGUIDFromPath_Server(NewGUID, PathName, CachedObj->outerguid(), 0, false, false);
				}
				else
				{
					// Fix 'Netguid mismatch' issue
					if (FNetGuidCacheObject* ExistingCacheObjectPtr = GuidCache->ObjectLookup.Find(NewGUID))
					{
						if (ExistingCacheObjectPtr->Object != nullptr && GuidCache->NetGUIDLookup.FindRef(ExistingCacheObjectPtr->Object) != NewGUID)
						{
							FNetworkGUID CurrentNetGUID = GuidCache->NetGUIDLookup.FindRef(ExistingCacheObjectPtr->Object);
							if (CurrentNetGUID != NewGUID)
							{
								GuidCache->NetGUIDLookup.Emplace(ExistingCacheObjectPtr->Object, NewGUID);
								UE_LOG(LogChanneld, Verbose, TEXT("[Client] Updated NetGUIDLookup for %s: %d -> %d"), *ExistingCacheObjectPtr->Object->GetName(), CurrentNetGUID.Value, NewGUID.Value);
							}
						}
					}

					GuidCache->RegisterNetGUIDFromPath_Client(NewGUID, PathName, CachedObj->outerguid(), 0, false, false);
				}
				UObject* NewObj = GuidCache->GetObjectFromNetGUID(NewGUID, false);
				// Only happens to static objects
				if (NewGUID == NetGUID)
				{
					check(NetGUID.IsStatic());
					Obj = NewObj;
				}
				UE_LOG(LogChanneld, Verbose, TEXT("[Client] Registered NetGUID %d (%d) from path: %s"), NewGUID.Value, ChanneldUtils::GetNativeNetId(NewGUID.Value), *PathName);
			}
			// v0.7.3 - end
			*/

			// Use the bunch to deserialize the object
			if (!IsValid(Obj))
			{
				if (Ref->bunchbitsnum() > 0)
				{
					FInBunch InBunch(Connection, (uint8*)Ref->netguidbunch().data(), Ref->bunchbitsnum());
					auto PackageMap = CastChecked<UPackageMapClient>(Connection->PackageMap);

					UActorChannel* Channel = (UActorChannel*)Connection->CreateChannelByName(NAME_Actor, EChannelCreateFlags::None);//EChannelCreateFlags::OpenedLocally);//
					UE_LOG(LogChanneld, VeryVerbose, TEXT("[Client] ActorChannels: %d"), Connection->ActorChannelsNum());
					AActor* Actor;
					//-----------------------------------------
					// Copied from UActorChannel::ProcessBunch
					//-----------------------------------------
					if (ClientConn ? SerializeNewActor_Server(ClientConn, PackageMap, GuidCache, InBunch, Channel, Actor) : PackageMap->SerializeNewActor(InBunch, Channel, Actor))
					{
						Channel->SetChannelActor(Actor, ESetChannelActorFlags::SkipReplicatorCreation);
						// Setup NetDriver, etc.
						Channel->NotifyActorChannelOpen(Actor, InBunch);

						/* Doesn't work: to turn off the warning in AActor::PostNetInit, add [Core.Log]LogActor=Error to DefaultEngine.ini
						// HACK: turn off the warning in AActor::PostNetInit temporarily
						GEngine->Exec(nullptr, TEXT("log Actor off"));
						*/
						// After all properties have been initialized, call PostNetInit. This should call BeginPlay() so initialization can be done with proper starting values.
						Actor->PostNetInit();
						// GEngine->Exec(nullptr, TEXT("log Actor on"));

						UE_LOG(LogChanneld, Verbose, TEXT("[Client] Created new actor '%s' with NetGUID %d (%d)"), *Actor->GetName(), GuidCache->GetNetGUID(Actor).Value, ChanneldUtils::GetNativeNetId(Ref->netguid()));

						/* Called in UChanneldNetDriver::NotifyActorDestroyed 
						// Remove the channel after using it
						//Channel->ConditionalCleanUp(true, EChannelCloseReason::Destroyed);
						*/
						
						if (ClientConn && ClientConn == NetConnForSpawn)
						{
							// Always remember to reset the NetConnForSpawn after using it.
							ResetNetConnForSpawn();
						}
						
						Obj = Actor;
					}
					else
					{
						// FNetGuidCacheObject* CacheObjectPtr = GuidCache->ObjectLookup.Find( NetGUID );
						UE_LOG(LogChanneld, Warning, TEXT("Failed to serialize new actor, cached object path: %s"), *FStaticGuidRegistry::GetStaticObjectExportedPathName(NetGUID.Value));
					}
				}
				// else if (Ref->has_classpath())
			}
		}

		if (!IsValid(Obj) && Ref->has_classpath())
		{
			FString PathName = UTF8_TO_TCHAR(Ref->classpath().c_str());
			
			// // Static object with bunch data (somehow)
			// if (NetGUID.IsStatic())
			// {
			// 	UE_LOG(LogChanneld, Warning, TEXT("Try to load static object with bunch data, netId: %u, path: %s, bits: %d"), NetGUID.Value, *PathName, Ref->bunchbitsnum());
			// }
			// else
			{
				if (auto ObjClass = LoadObject<UClass>(nullptr, *PathName))
				{
					if (ObjClass->IsChildOf<AActor>())
					{
						FActorSpawnParameters SpawnParams;
						SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
						Obj = World->SpawnActor(ObjClass, nullptr, nullptr, SpawnParams);	
					}
					else
					{
						Obj = NewObject<UObject>(GetTransientPackage(), ObjClass);
					}

					if (NetGUID.IsDynamic())
					{
						if (GuidCache->IsNetGUIDAuthority())
						{
							// GuidCache->RegisterNetGUIDFromPath_Server(NetGUID, PathName, 0, 0, false, false);
							if (!GuidCache->ObjectLookup.Contains(NetGUID))
							{
								// GuidCache->RegisterNetGUID_Server(NetGUID, Obj);
								RegisterNetGUID_Server_NoWarning(GuidCache, NetGUID, Obj);
							}
						}
						else
						{
							// GuidCache->RegisterNetGUIDFromPath_Client(NetGUID, PathName, 0, 0, false, false);
							GuidCache->RegisterNetGUID_Client(NetGUID, Obj);
						}
					}
					else
					{
						FNetGuidCacheObject CacheObject;
						CacheObject.Object = Obj;
						CacheObject.OuterGUID = FStaticGuidRegistry::GetStaticObjectExportedOuterGUID(NetGUID.Value);
						// Use the short name here so it can pass the check in FNetGUIDCache::GetObjectFromNetGUID
						CacheObject.PathName = Obj->GetFName();//FName(PathName);
						NetDriver->GuidCache->RegisterNetGUID_Internal(NetGUID, CacheObject);
						UE_LOG( LogNetPackageMap, Log, TEXT( "RegisterNetGUID_Internal: NetGUID: %u, PathName: %s" ), NetGUID.Value, *PathName);
					}
					
					// Increase the NetID counter to avoid duplicate NetID in GetOrAssignNetGUID -> AssignNewNetGUID_Server
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
					FNetGUIDCacheCopy* GuidCacheCopy = (FNetGUIDCacheCopy*)GuidCache.Get();
					++GuidCacheCopy->NetworkGuidIndex[NetGUID.IsStatic()];
#else
					++GuidCache->UniqueNetIDs[NetGUID.IsStatic()];
#endif

					if (AActor* Actor = Cast<AActor>(Obj))
					{
						// Triggers UChanneldNetDriver::NotifyActorChannelOpen
						World->GetNetDriver()->NotifyActorChannelOpen(nullptr, Actor);
						// After all properties have been initialized, call PostNetInit. This should call BeginPlay() so initialization can be done with proper starting values.
						Actor->PostNetInit();
					}
					
					// Fix the mismatch in the GuidCache
					if (FNetGuidCacheObject* NetIdMatchedCacheObjPtr = GuidCache->ObjectLookup.Find(NetGUID))
					{
						if (NetIdMatchedCacheObjPtr->Object != Obj)
						{
							FNetworkGUID RecoveredNetId = GuidCache->NetGUIDLookup.FindRef(Obj);
							FNetGuidCacheObject RecoveredCacheObj = GuidCache->ObjectLookup.FindRef(RecoveredNetId);
							FNetGuidCacheObject NetIdMatchedCacheObj = *NetIdMatchedCacheObjPtr;
				
							GuidCache->ObjectLookup.Emplace(NetGUID, RecoveredCacheObj);
							GuidCache->ObjectLookup.Emplace(RecoveredNetId, NetIdMatchedCacheObj);
							GuidCache->NetGUIDLookup.Emplace(RecoveredCacheObj.Object, NetGUID);
							GuidCache->NetGUIDLookup.Emplace(NetIdMatchedCacheObj.Object, RecoveredNetId);
							UE_LOG(LogChanneld, Verbose, TEXT("[Server] Update unmatched NetId %d <-> %d of path: %s <-> %s"), NetGUID.Value, RecoveredNetId.Value, *NetIdMatchedCacheObj.PathName.ToString(), *RecoveredCacheObj.PathName.ToString());
						}
					}
				}
				else
				{
					UE_LOG(LogChanneld, Warning, TEXT("ChanneldUtils::GetObjectByRef: Failed to load class from path: %s"), *PathName);
				}
			}
		}
	}

	if (!IsValid(Obj))
	{
		bNetGUIDUnmapped = true;
		// Only throw warning when failed to create the object.
		if (bCreateIfNotInCache)
		{
			UE_LOG(LogChanneld, Warning, TEXT("ChanneldUtils::GetObjectByRef: Failed to create object from NetGUID: %u (%u)"), NetGUID.Value, ChanneldUtils::GetNativeNetId(Ref->netguid()));
		}
	}
	else
	{
		// Only cache the full-exported UnrealObjectRef
		if (Ref->bunchbitsnum() > 0 && !ObjRefCache.Contains(NetGUID.Value))
		{
			auto Cached = MakeShared<unrealpb::UnrealObjectRef>();
			Cached->CopyFrom(*Ref);
			ObjRefCache.Emplace(NetGUID.Value, Cached);
			UE_LOG(LogChanneld, Verbose, TEXT("Cached ObjRef: %d, bunch bits: %d"), NetGUID.Value, Ref->bunchbitsnum());
		}
	}
	
	return Obj;
}

TSharedRef<unrealpb::UnrealObjectRef> ChanneldUtils::GetRefOfObject(UObject* Obj, UNetConnection* Connection /* = nullptr*/, bool bFullExport /*= false*/, UWorld* World /* = nullptr*/)
{
	auto ObjRef = MakeShared<unrealpb::UnrealObjectRef>();

	if (!Obj)
	{
		return ObjRef;
	}

	// CDO: treat it as an asset object
	if (Obj == Obj->GetClass()->ClassDefaultObject)
	{
		ObjRef->set_classpath(std::string(TCHAR_TO_UTF8(*Obj->GetPathName())));
		return ObjRef;
	}

	// Non-asset should have a valid world
	if (!World)
	{
		World = Obj->GetWorld();
	}
	if (!World)
	{
		return ObjRef;
	}

	auto GuidCache = World->GetNetDriver()->GuidCache;
	auto NetGUID = GuidCache->GetNetGUID(Obj);
	ObjRef->set_netguid(NetGUID.Value);

	// Static object's path name can be found locally
	if (NetGUID.IsStatic())
	{
		return ObjRef;
	}

	// If the dynamic object have already been full-exported to the client, just send the NetGUID
	if (!bFullExport)
	{
		if (!NetGUID.IsValid())
		{
			UE_LOG(LogChanneld, Warning, TEXT("ChanneldUtils::GetRefOfObject: Failed to get NetGUID of %s, Addr: %llu"), *Obj->GetPathName(), (uint64)Obj);
		}
		return ObjRef;
	}

	// Used the cached ObjRef so it won't be full-exported again
	if (const auto Cached = ObjRefCache.Find(NetGUID.Value))
	{
		UE_LOG(LogChanneld, VeryVerbose, TEXT("Use cached ObjRef: %d"), NetGUID.Value);
		return *Cached;
	}

	// Always set the classpath as it will be used in USpatialChannelDataView::CheckUnspawnedObject
	ObjRef->set_classpath(std::string(TCHAR_TO_UTF8(*Obj->GetClass()->GetPathName())));
	
	if (Obj->IsA<AActor>() && GuidCache->IsNetGUIDAuthority())
	{
		auto Actor = Cast<AActor>(Obj);
		if (Connection == nullptr)
		{
			Connection = Actor->GetNetConnection();
		}
		if (Connection == nullptr)
		{
			Connection = NetConnForSpawn;
		}
		if (!IsValid(Connection))
		{
			UE_LOG(LogChanneld, Log, TEXT("ChanneldUtils::GetRefOfObject: Unable to full-export '%s' as it doesn't have valid NetConnection"), *Obj->GetName());
			return ObjRef;
		}
		
		ObjRef->set_owningconnid(CastChecked<UChanneldNetConnection>(Connection)->GetConnId());
		
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
				UActorChannel* Channel = Connection->FindActorChannelRef(Actor);
				bool bFoundActorChannel = true;
				if (!Channel)
				{
					bFoundActorChannel = false;
					Channel = (UActorChannel*)Connection->CreateChannelByName(NAME_Actor, EChannelCreateFlags::None);
					UE_LOG(LogChanneld, VeryVerbose, TEXT("[Server] Created actor channel of %s for full-export, channels num: %d"), *GetNameSafe(Obj), Connection->ActorChannelsNum());
					Channel->SetChannelActor(Actor, ESetChannelActorFlags::None);
					PackageMap->SerializeNewActor(Ar, Channel, Actor);
					Actor->OnSerializeNewActor(Ar);
				}
				else
				{
					UE_LOG(LogChanneld, VeryVerbose, TEXT("[Server] Re-use actor channel %s for full-export."), *GetNameSafe(Obj));
					PackageMap->SerializeObject(Ar, Obj->GetClass(), Obj);
				}
				//--------------------------------------------------

				NetGUID = GuidCache->GetNetGUID(Obj);

				/* v0.7.3 - static objects are pre-registered on both server and client, so there's no need to register and send here.
				TSet<FNetworkGUID> NewGUIDs;
				PackageMap->NetGUIDExportCountMap.GetKeys(NewGUIDs);
				// Find the newly-registered NetGUIDs during SerializeNewActor()
				NewGUIDs = NewGUIDs.Difference(OldGUIDs);

				bool HaveGuidReassignment = false;
				for (FNetworkGUID& NewGUID : NewGUIDs)
				{
					// Don't send the target NetGUID in the context if it's dynamic
					if (NewGUID == NetGUID && NetGUID.IsDynamic())
						continue;

					auto NewCachedObj = GuidCache->GetCacheObject(NewGUID);
					auto Context = ObjRef->add_context();

					// Examine for pre-exported; replace if present and only fill exported id.
					if (NewGUID.IsStatic())
					{
						if (NewGUID.Value < GenManager_ChanneldStaticObjectExportStartID)
						{
							UObject* NewObj = NewCachedObj->Object.Get();
							const uint32 ExportedNetGUID = FStaticGuidRegistry::GetStaticObjectExportedNetGUID(NewObj->GetPathName());
							if (ExportedNetGUID != 0)
							{
								FStaticGuidRegistry::RegisterStaticObjectNetGUID_Authority(NewObj, ExportedNetGUID);
								NewGUID = FNetworkGUID(ExportedNetGUID);
								HaveGuidReassignment = true;
							}
						}
						if (NewGUID.Value >= GenManager_ChanneldStaticObjectExportStartID)
						{
							Context->set_netguid(NewGUID.Value);
							continue;
						}
					}

					Context->set_netguid(NewGUID.Value);
					Context->set_pathname(std::string(TCHAR_TO_UTF8(*NewCachedObj->PathName.ToString())));
					Context->set_outerguid(NewCachedObj->OuterGUID.Value);
					UE_LOG(LogChanneld, Verbose, TEXT("[Server] Send registered NetGUID %d with path: %s"), NewGUID.Value, *NewCachedObj->PathName.ToString());
				}

				// Due to static object id updates, The bunch needs to be repackaged to use the exported id
				if (HaveGuidReassignment)
				{
					Ar.Reset();
					Ar.bReliable = true;
					PackageMap->SerializeNewActor(Ar, Channel, Actor);
					Actor->OnSerializeNewActor(Ar);
				}
				// v0.7.3 - end
				*/

				ObjRef->set_netguid(NetGUID.Value);
				ObjRef->set_netguidbunch(Ar.GetData(), Ar.GetNumBytes());
				ObjRef->set_bunchbitsnum(Ar.GetNumBits());

				if (!bFoundActorChannel)
				{
					// Remove the channel after using it
					Channel->ConditionalCleanUp(true, EChannelCloseReason::Destroyed);
				}

				if (Connection == NetConnForSpawn)
				{
					// Clear the export map and ack state so everytime we can get a full export.
					ResetNetConnForSpawn();
				}

				// Only cache the full-exported UnrealObjectRef
				ObjRefCache.Emplace(NetGUID.Value, ObjRef);
				UE_LOG(LogChanneld, Verbose, TEXT("Cached ObjRef: %d, bunch bits: %d"), NetGUID.Value, ObjRef->bunchbitsnum());
			}
			//else
			//{
			//	NetGUID = GuidCache->GetOrAssignNetGUID(Obj);
			//}
		}
		else
		{
			UE_LOG(LogChanneld, Warning, TEXT("ChanneldUtils::GetRefOfObject: Failed to get the ref of %s: the Actor's NetConnection has no PackageMapClient"), *Obj->GetName());
		}
	}

	// ObjRef.set_connid(GEngine->GetEngineSubsystem<UChanneldConnection>()->GetConnId());
	return ObjRef;
}

UNetConnection* ChanneldUtils::GetActorNetConnection(const AActor* Actor)
{
	auto World = GWorld;
	if (!World || !Actor)
	{
		return nullptr;
	}

	UNetConnection* Conn = nullptr;
	if (const auto NetDriver = World->GetNetDriver())
	{
		if (NetDriver->IsServer())
		{
			Conn = Actor->GetNetConnection();
			if (!Conn)
			{
				Conn = NetConnForSpawn;
			}
		}
		else
		{
			Conn = NetDriver->ServerConnection;
		}
	}
	return Conn;
}

void ChanneldUtils::ResetNetConnForSpawn()
{
	auto PacketMapClient = CastChecked<UPackageMapClient>(NetConnForSpawn->PackageMap);
	PacketMapClient->NetGUIDExportCountMap.Empty();
	const static FPackageMapAckState EmptyAckStatus;
	PacketMapClient->RestorePackageMapExportAckStatus(EmptyAckStatus);
}

UActorComponent* ChanneldUtils::GetActorComponentByRef(const unrealpb::ActorComponentRef* Ref, UWorld* World, bool bCreateIfNotInCache, UChanneldNetConnection* ClientConn)
{
	bool bNetGUIDUnmapped;
	return GetActorComponentByRefChecked(Ref, World, bNetGUIDUnmapped, bCreateIfNotInCache, ClientConn);
}

UActorComponent* ChanneldUtils::GetActorComponentByRefChecked(const unrealpb::ActorComponentRef* Ref, UWorld* World, bool& bNetGUIDUnmapped, bool bCreateIfNotInCache, UChanneldNetConnection* ClientConn)
{
	if (!Ref || !World)
	{
		return nullptr;
	}

	AActor* Actor = Cast<AActor>(GetObjectByRef(&Ref->owner(), World, bNetGUIDUnmapped, bCreateIfNotInCache, ClientConn));
	if (!Actor)
	{
		// UE_LOG(LogChanneld, Warning, TEXT("Cannot get the owner actor of component by ref: %s"), UTF8_TO_TCHAR(Ref->DebugString().c_str()));
		return nullptr;
	}

	FName CompName = FName(UTF8_TO_TCHAR(Ref->compname().c_str()));
	/* GetDefaultSubobjectByName() doesn't work for dynamic components (e.g. CollisionComponent in Landscape)
	UObject* Comp = Actor->GetDefaultSubobjectByName(CompName);
	if (Comp)
	{
		return Cast<UActorComponent>(Comp);
	}
	*/
	for (auto Comp : Actor->GetComponents())
	{
		if (Comp->GetFName() == CompName)
		{
			return Cast<UActorComponent>(Comp);
		}
	}

	UE_LOG(LogChanneld, Warning, TEXT("Cannot find component '%s' of actor %s"), *CompName.ToString(), *Actor->GetName());
	return nullptr;
}

unrealpb::ActorComponentRef ChanneldUtils::GetRefOfActorComponent(UActorComponent* Comp, UNetConnection* Connection, bool bFullExportOwner)
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

	CompRef.mutable_owner()->MergeFrom(*GetRefOfObject(Comp->GetOwner(), Connection, bFullExportOwner));
	CompRef.set_compname(std::string(TCHAR_TO_UTF8(*Comp->GetFName().ToString())));
	return CompRef;
}

bool ChanneldUtils::SerializeNewActor_Server(UNetConnection* Connection, UPackageMapClient* PackageMap, TSharedPtr<FNetGUIDCache> GuidCache, FArchive& Ar, UActorChannel* Channel, AActor*& Actor)
{
	LLM_SCOPE(ELLMTag::EngineMisc);

	UE_LOG( LogNetPackageMap, VeryVerbose, TEXT( "SerializeNewActor START" ) );

	uint8 bIsClosingChannel = 0;

	if (Ar.IsLoading() )
	{
		FInBunch* InBunch = (FInBunch*)&Ar;
		bIsClosingChannel = InBunch->bClose;		// This is so we can determine that this channel was opened/closed for destruction
		UE_LOG(LogNetPackageMap, Log, TEXT("UPackageMapClient::SerializeNewActor BitPos: %d"), InBunch->GetPosBits() );

		PackageMap->ResetTrackedSyncLoadedGuids();
	}

	NET_CHECKSUM(Ar);

	FNetworkGUID NetGUID;
	UObject *NewObj = Actor;
	PackageMap->SerializeObject(Ar, AActor::StaticClass(), NewObj, &NetGUID);

	if ( Ar.IsError() )
	{
		UE_LOG( LogNetPackageMap, Error, TEXT( "UPackageMapClient::SerializeNewActor: Ar.IsError after SerializeObject 1" ) );
		return false;
	}

	bool bFilterGuidRemapping = true;//(CVarFilterGuidRemapping.GetValueOnAnyThread() > 0);
	if (!bFilterGuidRemapping)
	{
		if ( GuidCache.IsValid() )
		{
			if (ensureMsgf(NetGUID.IsValid(), TEXT("Channel tried to add an invalid GUID to the import list: %s"), *Channel->Describe()))
			{
				GuidCache->ImportedNetGuids.Add( NetGUID );
			}
		}
	}

	Channel->ActorNetGUID = NetGUID;

	Actor = Cast<AActor>(NewObj);

	// When we return an actor, we don't necessarily always spawn it (we might have found it already in memory)
	// The calling code may want to know, so this is why we distinguish
	bool bActorWasSpawned = false;

	if ( Ar.AtEnd() && NetGUID.IsDynamic() )
	{
		// This must be a destruction info coming through or something is wrong
		// If so, we should be closing the channel
		// This can happen when dormant actors that don't have channels get destroyed
		// Not finding the actor can happen if the client streamed in this level after a dynamic actor has been spawned and deleted on the server side
		if ( bIsClosingChannel == 0 )
		{
			UE_LOG( LogNetPackageMap, Error, TEXT( "UPackageMapClient::SerializeNewActor: bIsClosingChannel == 0 : %s [%s]" ), *GetNameSafe(Actor), *NetGUID.ToString() );
			Ar.SetError();
			return false;
		}

		UE_LOG( LogNetPackageMap, Log, TEXT( "UPackageMapClient::SerializeNewActor:  Skipping full read because we are deleting dynamic actor: %s" ), *GetNameSafe(Actor) );
		return false;		// This doesn't mean an error. This just simply means we didn't spawn an actor.
	}

	if (bFilterGuidRemapping)
	{
		// Do not mark guid as imported until we know we aren't deleting it
		if ( GuidCache.IsValid() )
		{
			if (ensureMsgf(NetGUID.IsValid(), TEXT("Channel tried to add an invalid GUID to the import list: %s"), *Channel->Describe()))
			{
				GuidCache->ImportedNetGuids.Add(NetGUID);
			}
		}
	}

	if ( NetGUID.IsDynamic() )
	{
		UObject* Archetype = nullptr;
		UObject* ActorLevel = nullptr;
		FVector Location;
		FVector Scale;
		FVector Velocity;
		FRotator Rotation;
		bool SerSuccess;

		if (Ar.IsSaving())
		{
			// ChildActor's need to be spawned from the ChildActorTemplate otherwise any non-replicated 
			// customized properties will be incorrect on the Client.
			if (UChildActorComponent* CAC = Actor->GetParentComponent())
			{
				Archetype = CAC->GetChildActorTemplate();
			}
			if (Archetype == nullptr)
			{
				Archetype = Actor->GetArchetype();
			}
			ActorLevel = Actor->GetLevel();

			check( Archetype != nullptr );
			check( Actor->NeedsLoadForClient() );			// We have no business sending this unless the client can load
			check( Archetype->NeedsLoadForClient() );		// We have no business sending this unless the client can load

			const USceneComponent* RootComponent = Actor->GetRootComponent();

			if (RootComponent)
			{
				Location = FRepMovement::RebaseOntoZeroOrigin(Actor->GetActorLocation(), Actor);
			} 
			else
			{
				Location = FVector::ZeroVector;
			}
			Rotation = RootComponent ? Actor->GetActorRotation() : FRotator::ZeroRotator;
			Scale = RootComponent ? Actor->GetActorScale() : FVector::OneVector;
			Velocity = RootComponent ? Actor->GetVelocity() : FVector::ZeroVector;
		}

		FNetworkGUID ArchetypeNetGUID;
		PackageMap->SerializeObject(Ar, UObject::StaticClass(), Archetype, &ArchetypeNetGUID);

		if (Ar.IsSaving() || (Connection && (Connection->EngineNetworkProtocolVersion >= EEngineNetworkVersionHistory::HISTORY_NEW_ACTOR_OVERRIDE_LEVEL)))
		{
			PackageMap->SerializeObject(Ar, ULevel::StaticClass(), ActorLevel);
		}

#if WITH_EDITOR
		UObjectRedirector* ArchetypeRedirector = Cast<UObjectRedirector>(Archetype);
		if (ArchetypeRedirector)
		{
			// Redirectors not supported
			Archetype = nullptr;
		}
#endif // WITH_EDITOR

		if ( ArchetypeNetGUID.IsValid() && Archetype == NULL )
		{
			const FNetGuidCacheObject* ExistingCacheObjectPtr = GuidCache->ObjectLookup.Find( ArchetypeNetGUID );

			if ( ExistingCacheObjectPtr != NULL )
			{
				UE_LOG( LogNetPackageMap, Error, TEXT( "UPackageMapClient::SerializeNewActor. Unresolved Archetype GUID. Path: %s, NetGUID: %s." ), *ExistingCacheObjectPtr->PathName.ToString(), *ArchetypeNetGUID.ToString() );
			}
			else
			{
				UE_LOG( LogNetPackageMap, Error, TEXT( "UPackageMapClient::SerializeNewActor. Unresolved Archetype GUID. Guid not registered! NetGUID: %s." ), *ArchetypeNetGUID.ToString() );
			}
		}

		// SerializeCompressedInitial
		// only serialize the components that need to be serialized otherwise default them
		bool bSerializeLocation = false;
		bool bSerializeRotation = false;
		bool bSerializeScale = false;
		bool bSerializeVelocity = false;

		{			
			// Server is serializing an object to be sent to a client
			if (Ar.IsSaving())
			{
				// We use 0.01f for comparing when using quantization, because we will only send a single point of precision anyway.
				// We could probably get away with 0.1f, but that may introduce edge cases for rounding.
				static constexpr float Epsilon_Quantized = 0.01f;
					
				// We use KINDA_SMALL_NUMBER for comparing when not using quantization, because that's the default for FVector::Equals.
				static constexpr float Epsilon = KINDA_SMALL_NUMBER;

				bSerializeLocation = !Location.Equals(FVector::ZeroVector, Epsilon_Quantized);
				bSerializeVelocity = !Velocity.Equals(FVector::ZeroVector, Epsilon_Quantized);
				bSerializeScale = !Scale.Equals(FVector::OneVector, Epsilon);

				// We use 0.001f for Rotation comparison to keep consistency with old behavior.
				bSerializeRotation = !Rotation.IsNearlyZero(0.001f);
					
			}

			auto ConditionallySerializeQuantizedVector = [&Ar, &SerSuccess, PackageMap](
				FVector& InOutValue,
				const FVector& DefaultValue,
				bool bShouldQuantize,
				bool& bWasSerialized)
			{
				Ar.SerializeBits(&bWasSerialized, 1);
				if (bWasSerialized)
				{
					if (Ar.EngineNetVer() < HISTORY_OPTIONALLY_QUANTIZE_SPAWN_INFO)
					{
						bShouldQuantize = true;
					}
					else
					{
						Ar.SerializeBits(&bShouldQuantize, 1);
					}

					if (bShouldQuantize)
					{
						FVector_NetQuantize10 Temp = InOutValue;
						Temp.NetSerialize(Ar, PackageMap, SerSuccess);
						InOutValue = Temp;
					}
					else
					{
						Ar << InOutValue;
					}
				}
				else
				{
					InOutValue = DefaultValue;
				}
			};

			ConditionallySerializeQuantizedVector(Location, FVector::ZeroVector, true, bSerializeLocation);

			Ar.SerializeBits(&bSerializeRotation, 1);
			if (bSerializeRotation)
			{
				Rotation.NetSerialize(Ar, PackageMap, SerSuccess);
			}
			else
			{
				Rotation = FRotator::ZeroRotator;
			}

			ConditionallySerializeQuantizedVector(Scale, FVector::OneVector, false, bSerializeScale);
			ConditionallySerializeQuantizedVector(Velocity, FVector::ZeroVector, true, bSerializeVelocity);
		}

		if ( Ar.IsLoading() )
		{
			// Spawn actor if necessary (we may have already found it if it was dormant)
			if ( Actor == NULL )
			{
				if ( Archetype )
				{
					// For streaming levels, it's possible that the owning level has been made not-visible but is still loaded.
					// In that case, the level will still be found but the owning world will be invalid.
					// If that happens, wait to spawn the Actor until the next time the level is streamed in.
					// At that point, the Server should resend any dynamic Actors.
					ULevel* SpawnLevel = Cast<ULevel>(ActorLevel);
					if (SpawnLevel == nullptr || SpawnLevel->GetWorld() != nullptr)
					{
						FActorSpawnParameters SpawnInfo;
						SpawnInfo.Template = Cast<AActor>(Archetype);
						SpawnInfo.OverrideLevel = SpawnLevel;
						SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
						// SpawnInfo.bRemoteOwned = true;
						SpawnInfo.bNoFail = true;

						UWorld* World = Connection->Driver->GetWorld();
						FVector SpawnLocation = FRepMovement::RebaseOntoLocalOrigin(Location, World->OriginLocation);
						Actor = World->SpawnActorAbsolute(Archetype->GetClass(), FTransform(Rotation, SpawnLocation), SpawnInfo);
						if (Actor)
						{
							// Velocity was serialized by the server
							if (bSerializeVelocity)
							{
								Actor->PostNetReceiveVelocity(Velocity);
							}

							// Scale was serialized by the server
							if (bSerializeScale)
							{
								Actor->SetActorScale3D(Scale);
							}

							/* Original code: only for client. Check !IsNetGUIDAuthority() will fail.
							GuidCache->RegisterNetGUID_Client(NetGUID, Actor);
							*/

							// Server may already has the actor spawned and cached. Remove it before registering.
							GuidCache->NetGUIDLookup.Remove(Actor);
							GuidCache->ObjectLookup.Remove(NetGUID);
								
							GuidCache->RegisterNetGUID_Server(NetGUID, Actor);

							bActorWasSpawned = true;
						}
						else
						{
							UE_LOG(LogNetPackageMap, Warning, TEXT("SerializeNewActor: Failed to spawn actor for NetGUID: %s, Channel: %d"), *NetGUID.ToString(), Channel->ChIndex);
						}
					}
					else
					{
						UE_LOG(LogNetPackageMap, Log, TEXT("SerializeNewActor: Actor level has invalid world (may be streamed out). NetGUID: %s, Channel: %d"), *NetGUID.ToString(), Channel->ChIndex);
					}
				}
				else
				{
					UE_LOG(LogNetPackageMap, Error, TEXT("UPackageMapClient::SerializeNewActor Unable to read Archetype for NetGUID %s / %s"), *NetGUID.ToString(), *ArchetypeNetGUID.ToString() );
				}
			}
		}
	}
	else if ( Ar.IsLoading() && Actor == NULL )
	{
		// Do not log a warning during replay, since this is a valid case
		UDemoNetDriver* DemoNetDriver = Cast<UDemoNetDriver>(Connection->Driver);
		if (DemoNetDriver == nullptr)
		{
			UE_LOG( LogNetPackageMap, Log, TEXT( "SerializeNewActor: Failed to find static actor: FullNetGuidPath: %s, Channel: %d" ), *GuidCache->FullNetGUIDPath( NetGUID ), Channel->ChIndex );
		}

		if (bFilterGuidRemapping)
		{
			// Do not attempt to resolve this missing actor
			if ( GuidCache.IsValid() )
			{
				GuidCache->ImportedNetGuids.Remove( NetGUID );
			}
		}
	}

	if (Ar.IsLoading())
	{
		// PackageMap->ReportSyncLoadsForActorSpawn(Actor);
	}

	UE_LOG( LogNetPackageMap, Log, TEXT( "SerializeNewActor END: Finished Serializing. Actor: %s, FullNetGUIDPath: %s, Channel: %d, IsLoading: %i, IsDynamic: %i" ), Actor ? *Actor->GetName() : TEXT("NULL"), *GuidCache->FullNetGUIDPath( NetGUID ), Channel->ChIndex, (int)Ar.IsLoading(), (int)NetGUID.IsDynamic() );

	return bActorWasSpawned;
}

void ChanneldUtils::RegisterNetGUID_Server_NoWarning(TSharedPtr<FNetGUIDCache> GuidCache, const FNetworkGUID& NetGUID, UObject* Object)
{
	check( Object != NULL );
	check( GuidCache->IsNetGUIDAuthority() );				// Only the server should call this
	check( !Object->IsPendingKill() );
	check( !NetGUID.IsDefault() );
	check( !GuidCache->ObjectLookup.Contains( NetGUID ) );	// Server should never add twice

	FNetGuidCacheObject CacheObject;

	CacheObject.Object				= MakeWeakObjectPtr(const_cast<UObject*>(Object));
	CacheObject.OuterGUID			= GuidCache->GetOrAssignNetGUID( Object->GetOuter() );
	CacheObject.PathName			= Object->GetFName();
	CacheObject.NetworkChecksum		= GuidCache->GetNetworkChecksum( Object );
	CacheObject.bNoLoad				= !GuidCache->CanClientLoadObject( Object, NetGUID );
	CacheObject.bIgnoreWhenMissing	= CacheObject.bNoLoad;

	GuidCache->ObjectLookup.Emplace( NetGUID, CacheObject );

	if ( CacheObject.Object != NULL )
	{
		// check( !GuidCache->NetGUIDLookup.Contains( CacheObject.Object ) );

		// If we have an object, associate it with this guid now
		GuidCache->NetGUIDLookup.Emplace( CacheObject.Object, NetGUID );
	}
}

void ChanneldUtils::SetActorRoleByOwningConnId(AActor* Actor, Channeld::ConnectionId OwningConnId)
{
	UChanneldConnection* ConnToChanneld = GEngine->GetEngineSubsystem<UChanneldConnection>();
	ENetRole OldRole = Actor->GetLocalRole();
	if (ConnToChanneld->GetConnId() == OwningConnId)
	{
		Actor->SetRole(ROLE_AutonomousProxy);
	}
	else// if (Actor->GetLocalRole() == ROLE_AutonomousProxy)
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

	// DEBUG: Disable tick for simulated proxies
	if (GetMutableDefault<UChanneldSettings>()->bDisableSimulatedProxyTick && Actor->GetLocalRole() == ROLE_SimulatedProxy)
	{
		Actor->SetActorTickEnabled(false);
		Actor->ForEachComponent(true, [](UActorComponent* Comp)
		{
			Comp->SetComponentTickEnabled(false);
		});
	}
}

ENetRole ChanneldUtils::ServerGetActorNetRole(AActor* Actor)
{
	if (UWorld* World = Actor->GetWorld())
	{
		if (auto NetDriver = Cast<UChanneldNetDriver>(World->GetNetDriver()))
		{
			if (NetDriver->ChannelDataView.IsValid())
			{
				auto ChId = NetDriver->ChannelDataView->GetOwningChannelId(Actor);
				if (NetDriver->GetConnToChanneld()->OwnedChannels.Contains(ChId))
				{
					return ROLE_Authority;
				}
				else
				{
					return ROLE_SimulatedProxy;
				}
			}
		}
	}

	UE_LOG(LogChanneld, Warning, TEXT("ChanneldUtils::ServerGetActorNetRole failed. Actor: %s"), *Actor->GetName());
	return ROLE_None;
}
