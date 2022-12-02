#pragma once
#include "ReplicatedActorDecorator.h"

struct FReplicatorCode
{
	TSharedPtr<FReplicatedActorDecorator> Target;

	FString HeadFileName;
	FString HeadCode;

	FString CppFileName;
	FString CppCode;

	FString ProtoFileName;
	FString ProtoDefinitions;

	FString IncludeActorCode;
	FString RegisterReplicatorCode;
};

struct FReplicatorCodeBundle
{
	FString RegisterReplicatorFileCode;

	TArray<FReplicatorCode> ReplicatorCodes;
};

class REPLICATORGENERATOR_API FReplicatorCodeGenerator
{
public:
	bool RefreshModuleInfoByClassName();

	bool Generate(TArray<UClass*> TargetActors, FReplicatorCodeBundle& ReplicatorCodeBundle);
	bool GenerateActorCode(UClass* TargetActor, FReplicatorCode& ReplicatorCode, FString& ResultMessage);


protected:
	FString GetProtoMessageOfGlobalStruct();

	TMap<FString, FModuleInfo> ModuleInfoByClassName;

	inline void ProcessHeaderFiles(const TArray<FString>& Files, const FManifestModule& ManifestModule);

};
