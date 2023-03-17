// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class FArchive;

struct FModuleInfo
{
	FString Name;

	FString BaseDirectory;

	FString IncludeBase;

	FString RelativeToModule;

	FString GetRelativePath(const FString& FilePath )
	{
		return FilePath.Replace(*BaseDirectory, *Name, ESearchCase::CaseSensitive);
	}

	bool bIsBuildInEngine;
};

struct FManifestModule
{
	/** The name of the module */
	FString Name;

	/** Long package name for this module's UObject class */
	FString LongPackageName;

	/** Base directory of this module on disk */
	FString BaseDirectory;

	/** The directory to which #includes from this module should be relative */
	FString IncludeBase;

	/** Directory where generated include files should go */
	FString GeneratedIncludeDirectory;

	/** List of C++ public 'Classes' header files with UObjects in them (legacy) */
	TArray<FString> PublicUObjectClassesHeaders;

	/** List of C++ public header files with UObjects in them */
	TArray<FString> PublicUObjectHeaders;

	/** List of C++ private header files with UObjects in them */
	TArray<FString> PrivateUObjectHeaders;

	/** Base (i.e. extensionless) path+filename of where to write out the module's .generated.* files */
	FString GeneratedCPPFilenameBase;

	/** Whether or not to write out headers that have changed */
	bool SaveExportedHeaders;

	friend FArchive& operator<<(FArchive& Ar, FManifestModule& ManifestModule)
	{
		Ar << ManifestModule.Name;
		Ar << ManifestModule.LongPackageName;
		Ar << ManifestModule.BaseDirectory;
		Ar << ManifestModule.IncludeBase;
		Ar << ManifestModule.GeneratedIncludeDirectory;
		Ar << ManifestModule.PublicUObjectClassesHeaders;
		Ar << ManifestModule.PublicUObjectHeaders;
		Ar << ManifestModule.PrivateUObjectHeaders;
		Ar << ManifestModule.GeneratedCPPFilenameBase;
		Ar << ManifestModule.SaveExportedHeaders;

		return Ar;
	}
};


struct FManifest
{
	bool    IsGameTarget;
	FString RootLocalPath;
	FString RootBuildPath;
	FString TargetName;
	FString ExternalDependenciesFile;
	
	/** Ordered list of modules that define UObjects or UStructs, which we may need to generate
	    code for.  The list is in module dependency order, such that most dependent modules appear first. */
	TArray<FManifestModule> Modules;

	/**
	 * Loads a *.uhtmanifest from the specified filename.
	 *
	 * @param Filename The filename of the manifest to load.
	 * @param Success load success.
	 * @return The loaded module info.
	 */
	static FManifest LoadFromFile(const FString& Filename, bool& Success);

	friend FArchive& operator<<(FArchive& Ar, FManifest& Manifest)
	{
		Ar << Manifest.IsGameTarget;
		Ar << Manifest.RootLocalPath;
		Ar << Manifest.RootBuildPath;
		Ar << Manifest.TargetName;
		Ar << Manifest.Modules;

		return Ar;
	}
};
