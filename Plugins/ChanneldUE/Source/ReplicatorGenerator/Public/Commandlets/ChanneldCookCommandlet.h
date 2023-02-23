// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CookCommandlet.cpp: Commandlet for cooking content
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ChanneldCookCommandlet.generated.h"

class FSandboxPlatformFile;
class ITargetPlatform;

UCLASS(config=Editor)
class UChanneldCookCommandlet
	: public UCommandlet
{
	GENERATED_UCLASS_BODY()

	/** List of asset types that will force GC after loading them during cook */
	UPROPERTY(config)
	TArray<FString> FullGCAssetClassNames;

	/** If true, iterative cooking is being done */
	bool bIterativeCooking;
	/** Prototype cook-on-the-fly server */
	bool bCookOnTheFly; 
	/** Cook everything */
	bool bCookAll;
	/** Skip saving any packages in Engine/Content/Editor* UNLESS TARGET HAS EDITORONLY DATA (in which case it will save those anyway) */
	bool bSkipEditorContent;
	/** Save all cooked packages without versions. These are then assumed to be current version on load. This is dangerous but results in smaller patch sizes. */
	bool bUnversioned;
	/** Generate manifests for building streaming install packages */
	bool bGenerateStreamingInstallManifests;
	/** Error if we access engine content (useful for dlc) */
	bool bErrorOnEngineContentUse;
	/** Use historical serialization system for generating package dependencies (use for historical reasons only this method has been depricated, only affects cooked manifests) */
	bool bUseSerializationForGeneratingPackageDependencies;
	/** Only cook packages specified on commandline options (for debugging)*/
	bool bCookSinglePackage;
	/** Modification to bCookSinglePackage - cook transitive hard references in addition to the packages on the commandline */
	bool bKeepSinglePackageRefs;
	/** Should we output additional verbose cooking warnings */
	bool bVerboseCookerWarnings;
	/** only clean up objects which are not in use by the cooker when we gc (false will enable full gc) */
	bool bPartialGC;
	/** Do not cook any shaders.  Shader maps will be empty */
	bool bNoShaderCooking;
	/** All commandline tokens */
	TArray<FString> Tokens;
	/** All commandline switches */
	TArray<FString> Switches;
	/** All commandline params */
	FString Params;

	/**
	 * Cook on the fly routing for the commandlet
	 *
	 * @param  BindAnyPort					Whether to bind on any port or the default port.
	 * @param  Timeout						Length of time to wait for connections before attempting to close
	 * @param  bForceClose					Whether or not the server should always shutdown after a timeout or after a user disconnects
	 * @param  TargetPlatforms				The list of platforms that should be initialized at startup.  Other platforms will be initialized when first requested
	 *
	 * @return true on success, false otherwise.
	 */
	bool CookOnTheFly( FGuid InstanceId, int32 Timeout = 180, bool bForceClose = false, const TArray<ITargetPlatform*>& TargetPlatforms=TArray<ITargetPlatform*>() );

	/** Cooks for specified targets */
	bool CookByTheBook(const TArray<ITargetPlatform*>& Platforms);

	/**	Process deferred commands */
	void ProcessDeferredCommands();

public:

	//~ Begin UCommandlet Interface

	virtual int32 Main(const FString& CmdLineParams) override;
	
	//~ End UCommandlet Interface

};
