#include "StaticGuidRegistry.h"

#include "JsonObjectConverter.h"
#include "ChanneldTypes.h"
#include "Engine/PackageMapClient.h"

TMap<FString, uint32> StaticObjectExportNetGUID;
TMap<uint32, FString> StaticObjectExportPathName;
TMap<uint32, uint32> StaticObjectExportOuterExportID;

bool FStaticGuidRegistry::LoadExportedFile(const FString& FilePath)
{
	double StartTime = FPlatformTime::Seconds();

	FString JsonString;
	FStaticObjectCache Data;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogChanneld, Error, TEXT("Failed to load data from file: %s"), *FilePath);
		return false;
	}
	if (!FJsonObjectConverter::JsonObjectStringToUStruct<FStaticObjectCache>(JsonString, &Data, 0, 0))
	{
		UE_LOG(LogChanneld, Error, TEXT("Failed to parse json from file: %s"), *FilePath);
		return false;
	}

	for (auto& Info : Data.StaticObjects)
	{
		StaticObjectExportNetGUID.Add(Info.PathName, Info.NetID);
		StaticObjectExportPathName.Add(Info.NetID, Info.PathName);
		StaticObjectExportOuterExportID.Add(Info.NetID, Info.OuterID);
	}

	UE_LOG(LogChanneld, Display, TEXT("Successful load %d static objects, took %.3f seconds"), StaticObjectExportNetGUID.Num(), FPlatformTime::Seconds() - StartTime);
	return true;
}

void FStaticGuidRegistry::RegisterStaticObjects(UNetDriver* NetDriver)
{
	if (NetDriver == nullptr)
	{
		UE_LOG(LogChanneld, Error, TEXT("FStaticGuidRegistry::LoadAndRegisterStaticObjects: NetDriver is nullptr"));
		return;
	}

	double StartTime = FPlatformTime::Seconds();

	for (auto Pair : StaticObjectExportPathName)
	{
		FNetworkGUID NetGUID = Pair.Key;
		FString PathName = Pair.Value;
		FNetworkGUID OuterGUID = StaticObjectExportOuterExportID.FindRef(NetGUID.Value);
		
		if (NetDriver->GuidCache->ObjectLookup.Contains(NetGUID))
		{
			UE_LOG( LogNetPackageMap, Warning, TEXT( "Duplicated NetGUID found in ObjectLookup: %u, PathName: %s" ), NetGUID.Value, *PathName);
			continue;
		}

		// Remap name for PIE (should only be done on the client side)
		if (GEngine->NetworkRemapPath(NetDriver->ServerConnection, PathName, true))
		{
			// UE_LOG(LogChanneld, Verbose, TEXT("Remapped path name: %s -> %s"), *Pair.Value, *PathName);
		}

		if (auto Obj = FindObject<UObject>(nullptr, *PathName))
		{
			/* NEVER happens as the registration is done after UNetDriver::SetWorld
			// Some static object such as the level may already exist in the cache
			if (NetDriver->GuidCache->NetGUIDLookup.Remove(MakeWeakObjectPtr(const_cast<UObject*>(Obj))))
			{
				UE_LOG( LogNetPackageMap, Log, TEXT( "Duplicated Object found in NetGUIDLookup: %s, NetGUID: %u" ), *PathName, NetGUID.Value);
			}
			*/
			if (auto OldNetId = NetDriver->GuidCache->NetGUIDLookup.Find(Obj))
			{
				auto CachedObj = NetDriver->GuidCache->ObjectLookup.FindAndRemoveChecked(*OldNetId);
				UE_LOG( LogNetPackageMap, Log, TEXT( "Updating GuidCache, PathName: '%s', NetGUID: %u -> %u" ), *PathName, OldNetId->Value, NetGUID.Value);
				*OldNetId = NetGUID;
				NetDriver->GuidCache->ObjectLookup.Emplace(NetGUID, CachedObj);
				continue;
			}
	
			if (NetDriver->IsServer())
			{
				NetDriver->GuidCache->RegisterNetGUID_Server(NetGUID, Obj);
				UE_LOG( LogNetPackageMap, Log, TEXT( "RegisterNetGUID_Server: NetGUID: %u, PathName: %s" ), NetGUID.Value, *PathName);
			}
			else
			{
				/* RegisterNetGUID_Client only accept dynamic objects
				//NetDriver->GuidCache->RegisterNetGUID_Client(NetGUID, Obj);
				*/
		
				FNetGuidCacheObject CacheObject;
				CacheObject.Object = Obj;
				CacheObject.OuterGUID = OuterGUID;
				// Use the short name here so it can pass the check in FNetGUIDCache::GetObjectFromNetGUID
				CacheObject.PathName = Obj->GetFName();//FName(PathName);
				NetDriver->GuidCache->RegisterNetGUID_Internal(NetGUID, CacheObject);
				UE_LOG( LogNetPackageMap, Log, TEXT( "RegisterNetGUID_Internal: NetGUID: %u, PathName: %s" ), NetGUID.Value, *PathName);

			}
		}
		else
		{
			if (NetDriver->IsServer())
			{
				NetDriver->GuidCache->RegisterNetGUIDFromPath_Server(NetGUID, PathName, OuterGUID, 0, false, false);
			}
			else
			{
				NetDriver->GuidCache->RegisterNetGUIDFromPath_Client(NetGUID, PathName, OuterGUID, 0, false, false);
			}
		}
	}
	
	UE_LOG(LogChanneld, Display, TEXT("Registering static objects took %.3f seconds"), FPlatformTime::Seconds() - StartTime);
}

uint32 FStaticGuidRegistry::GetStaticObjectExportedNetGUID(const FString& PathName)
{
	if (uint32* ExportedNetGUIDPtr = StaticObjectExportNetGUID.Find(PathName))
	{
		return *ExportedNetGUIDPtr;
	}
	return 0;
}

FString FStaticGuidRegistry::GetStaticObjectExportedPathName(uint32 NetGUID)
{
	if (FString* PathName = StaticObjectExportPathName.Find(NetGUID))
	{
		return *PathName;
	}
	return FString();
}

uint32 FStaticGuidRegistry::GetStaticObjectExportedOuterGUID(uint32 NetGUID)
{
	if (uint32* OuterNetGUIDPtr = StaticObjectExportOuterExportID.Find(NetGUID))
	{
		return *OuterNetGUIDPtr;
	}
	return 0;
}

UObject* FStaticGuidRegistry::GetStaticObject(FNetworkGUID NetGUID, UNetDriver* NetDriver)
{
	FString PathName = GetStaticObjectExportedPathName(NetGUID.Value);
	if (GEngine->NetworkRemapPath(NetDriver->ServerConnection, PathName, true))
	{
		UE_LOG(LogChanneld, VeryVerbose, TEXT("Getting static object from remapped path name: %s -> %s"), *GetStaticObjectExportedPathName(NetGUID.Value), *PathName);
	}
	UObject* Obj = FindObject<UObject>(nullptr, *PathName);
	if (!IsValid((Obj)))
	{
		Obj = LoadObject<UObject>(nullptr, *PathName);
	}
	if (!IsValid((Obj)))
	{
		Obj = NetDriver->GuidCache->GetObjectFromNetGUID(NetGUID, false);
	}
	if (IsValid((Obj)))
	{
		UE_LOG(LogChanneld, VeryVerbose, TEXT("Loaded static object '%s' with NetGUID %u"), *PathName, NetGUID.Value);
	}
	else
	{
		UE_LOG(LogChanneld, Warning, TEXT("Failed to load static object '%s' with NetGUID %u"), *PathName, NetGUID.Value);
	}
	return Obj;
}

void FStaticGuidRegistry::RegisterStaticObjectNetGUID_Authority(UObject* Obj, uint32 ExportID)
{
	check(ExportID >= GenManager_ChanneldStaticObjectExportStartID);
	check(Obj);
	check(GWorld);
	check(GWorld->GetNetDriver());
	FNetGUIDCache* GuidCache = GWorld->GetNetDriver()->GuidCache.Get();
	
	if (Obj->GetOuter())
	{
		const uint32 OuterExportID = GetStaticObjectExportedNetGUID(Obj->GetPathName());
		if (OuterExportID >= GenManager_ChanneldStaticObjectExportStartID)
		{
			RegisterStaticObjectNetGUID_Authority(Obj->GetOuter(), OuterExportID);
		}
	}

	auto const NetGUID = FNetworkGUID(ExportID);
	auto CurrentGUID = GuidCache->GetNetGUID(Obj);
	if (NetGUID != CurrentGUID)
	{
		auto NetGUIDObjCache = GuidCache->ObjectLookup.Find(NetGUID);
		if (NetGUIDObjCache)
		{
			GuidCache->NetGUIDLookup.Remove(NetGUIDObjCache->Object);
			GuidCache->ObjectLookup.Remove(NetGUID);
		}
		GuidCache->NetGUIDLookup.Remove(Obj);
		GuidCache->ObjectLookup.Remove(CurrentGUID);
		GuidCache->RegisterNetGUID_Server(NetGUID, Obj);
	}
}

void FStaticGuidRegistry::RegisterStaticObjectNetGUID_NonAuthority(const UObject* Obj, const FString PathName, uint32 ExportID, UNetConnection* Connection, bool bRunningOnServer)
{
	check(ExportID >= GenManager_ChanneldStaticObjectExportStartID);
	check(Obj);
	check(GWorld);
	check(GWorld->GetNetDriver());
	FNetGUIDCache* GuidCache = GWorld->GetNetDriver()->GuidCache.Get();
	UObject* OuterObj = Obj->GetOuter();
	/*
	// Remap name for PIE
	FString PathName = Obj->GetPathName(OuterObj);
	GEngine->NetworkRemapPath(Connection, PathName, true);
	*/
	const FNetworkGUID NetGUID(ExportID);
	FNetworkGUID OuterNetGUID;
	if (OuterObj)
	{
		FString OuterPathName = OuterObj->GetPathName();
		uint32 OuterExportID = GetStaticObjectExportedNetGUID(OuterPathName);
		if (OuterExportID >= GenManager_ChanneldStaticObjectExportStartID)
		{
			RegisterStaticObjectNetGUID_NonAuthority(OuterObj, OuterPathName, OuterExportID, Connection, bRunningOnServer);
			OuterNetGUID = FNetworkGUID(OuterExportID);
		}
		else
		{
			OuterNetGUID = GuidCache->GetNetGUID(OuterObj);
		}
	}
	
	if (bRunningOnServer)
	{
		GuidCache->RegisterNetGUIDFromPath_Server(NetGUID, PathName, OuterNetGUID, 0, false, false);
	}
	else
	{
		GuidCache->RegisterNetGUIDFromPath_Client(NetGUID, PathName, OuterNetGUID, 0, false, false);
	}
}

UObject* FStaticGuidRegistry::TryLoadStaticObject(uint32 NetGUID, UNetConnection* Connection, bool bRunningOnServer)
{
	check(NetGUID >= GenManager_ChanneldStaticObjectExportStartID);
	UObject* NewObj = nullptr;
	FString* PathNamePtr = StaticObjectExportPathName.Find(NetGUID);
	if (ensure(PathNamePtr))
	{
		FString& PathName = *PathNamePtr;
		UE_LOG(LogChanneld, VeryVerbose, TEXT("Try load static object, PathName: %s"), *PathName);
		NewObj = FindObject<UObject>(nullptr, *PathName);
		if (NewObj == nullptr)
		{
			NewObj = LoadObject<UObject>(nullptr, *PathName);
		}
		if (!NewObj && FPackageName::IsValidLongPackageName(PathName))
		{
			UE_LOG(LogChanneld, Log, TEXT("LoadObject failed. Try load static package, PathName: %s"), *PathName);
			NewObj = LoadPackage(nullptr, *PathName, LOAD_None);
		}
		if (NewObj)
		{
			RegisterStaticObjectNetGUID_NonAuthority(NewObj, *PathNamePtr, NetGUID, Connection, bRunningOnServer);
			UE_LOG(LogChanneld, Verbose, TEXT("Try load static object success, PathName: %s"), *PathName);
		}
		else
		{
			UE_LOG(LogChanneld, Warning, TEXT("Try load static object failed, PathName: %s"), *PathName);
		}
	}

	return NewObj;
}
