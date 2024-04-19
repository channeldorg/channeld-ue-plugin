#pragma once

#include "CoreMinimal.h"

// Utility class for loading pre-exported NetGUID-StaticObject mapping,
// and register the static objects to the GuidCache.
//
// The mapping file is exported during the cook process when refreshing the replication cache.
//
// With this class, static objects can be replicated over network with only NetGUIDs,
// and the default FastArraySerializer can work properly with static objects.
class CHANNELDUE_API FStaticGuidRegistry
{
public:
	static bool LoadExportedFile(const FString& FilePath);
	static void LoadAndRegisterStaticObjects(UNetDriver* NetDriver);
	
	static uint32 GetStaticObjectExportedNetGUID(const FString& PathName);
	static FString GetStaticObjectExportedPathName(uint32 NetGUID);
	static UObject* GetStaticObject(FNetworkGUID NetGUID, UNetDriver* NetDriver);
	static void RegisterStaticObjectNetGUID_Authority(UObject* Obj, uint32 ExportID);
	static void RegisterStaticObjectNetGUID_NonAuthority(const UObject* Obj, const FString PathName, uint32 ExportID, UNetConnection* Connection, bool bRunningOnServer = false);
	static UObject* TryLoadStaticObject(uint32 NetGUID, UNetConnection* Connection, bool bRunningOnServer = false);
};