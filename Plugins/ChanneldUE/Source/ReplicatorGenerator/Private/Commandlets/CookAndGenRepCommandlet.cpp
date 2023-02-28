// Fill out your copyright notice in the Description page of Project Settings.


#include "Commandlets/CookAndGenRepCommandlet.h"

#include "ReplicatorGeneratorManager.h"
#include "ReplicatorGeneratorUtils.h"

UCookAndGenRepCommandlet::UCookAndGenRepCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCookAndGenRepCommandlet::Main(const FString& CmdLineParams)
{
	FReplicatorGeneratorManager& GeneratorManager = FReplicatorGeneratorManager::Get();
	GeneratorManager.StartGenerateReplicator();

	TSet<FSoftClassPath> LoadedRepClasses;

	ChanneldReplicatorGeneratorUtils::FReplicationActorFilter ObjLoadedListener(ChanneldReplicatorGeneratorUtils::EFilterRule::NeedToGenerateReplicator);
	ObjLoadedListener.StartListen();

	const FString AdditionalParam(TEXT(" -SkipShaderCompile"));
	FString NewCmdLine = CmdLineParams;
	NewCmdLine.Append(AdditionalParam);
	FCommandLine::Append(*AdditionalParam);

	int32 Result = Super::Main(CmdLineParams);
	ObjLoadedListener.StopListen();

	LoadedRepClasses.Append(ObjLoadedListener.LoadedRepClasses);
	TArray<const UClass*> TargetClasses;
	for (const FSoftClassPath& ObjSoftPath : LoadedRepClasses)
	{
		if (const UClass* LoadedClass = ObjSoftPath.TryLoadClass<UObject>())
		{
			TargetClasses.Add(LoadedClass);
		}
	}

	//Get parameter '-GoPackageImportPathPrefix' from command line
	FString GoPackageImportPathPrefix;
	FParse::Value(*CmdLineParams, TEXT("-GoPackageImportPathPrefix="), GoPackageImportPathPrefix);

	GeneratorManager.RemoveGeneratedCode();
	GeneratorManager.GeneratedReplicators(TargetClasses, GoPackageImportPathPrefix);

	GeneratorManager.StopGenerateReplicator();

	return Result;
}
