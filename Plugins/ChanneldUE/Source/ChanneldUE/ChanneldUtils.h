#pragma once

#include "CoreMinimal.h"
#include "unreal_common.pb.h"
#include "Engine/PackageMapClient.h"
#include "Engine/ActorChannel.h"
#include "ChanneldTypes.h"
#include "ChanneldConnection.h"
#include "ChanneldNetConnection.h"
#include "Engine/DemoNetDriver.h"

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
					if (ClientConn ? SerializeNewActor_Server(ClientConn, PackageMap, GuidCache, InBunch, Channel, Actor) : PackageMap->SerializeNewActor(InBunch, Channel, Actor))
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
						// Don't send the target NetGUID in the context if it's dynamic
						if (NewGUID == NetGUID && NetGUID.IsDynamic())
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
	static T* GetActorComponentByRef(const unrealpb::ActorComponentRef* Ref, UWorld* World, bool bCreateIfNotInCache = true, UChanneldNetConnection* ClientConn = nullptr)
	{
		bool bNetGUIDUnmapped;
		return GetActorComponentByRefChecked<T>(Ref, World, bNetGUIDUnmapped, bCreateIfNotInCache, ClientConn);
	}
	
	template<class T>
	static T* GetActorComponentByRefChecked(const unrealpb::ActorComponentRef* Ref, UWorld* World, bool& bNetGUIDUnmapped, bool bCreateIfNotInCache = true, UChanneldNetConnection* ClientConn = nullptr)
	{
		if (!Ref || !World)
		{
			return nullptr;
		}

		AActor* Actor = Cast<AActor>(GetObjectByRef(&Ref->owner(), World, bCreateIfNotInCache, ClientConn));
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

	/**
	 *	Standard method of serializing a new actor on server.
	 *		For static actors, this will just be a single call to SerializeObject, since they can be referenced by their path name.
	 *		For dynamic actors, first the actor's reference is serialized but will not resolve on clients since they haven't spawned the actor yet.
	 *		The actor archetype is then serialized along with the starting location, rotation, and velocity.
	 *		After reading this information, the client spawns this actor in the NetDriver's World and assigns it the NetGUID it read at the top of the function.
	 *
	 *		returns true if a new actor was spawned. false means an existing actor was found for the netguid.
	 */
	bool static SerializeNewActor_Server(UNetConnection* Connection, UPackageMapClient* PackageMap, TSharedPtr<FNetGUIDCache> GuidCache, FArchive& Ar, class UActorChannel *Channel, class AActor*& Actor)
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
};
