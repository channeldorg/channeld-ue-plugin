#pragma once
#include "ReplicatorCodeGenerator.h"

static const FString GenManager_GeneratedCodeDir = TEXT("ChanneldGenerated");

static const FName GenManager_ProtobufModuleName = TEXT("ProtobufEditor");

class REPLICATORGENERATOR_API FReplicatorGeneratorManager
{
private:
	FReplicatorCodeGenerator* CodeGenerator;
public:
	FReplicatorGeneratorManager();
	~FReplicatorGeneratorManager();

	static FReplicatorGeneratorManager& Get();


	bool GenerateReplicator(UClass* Target);

	bool WriteCodeFile(const FString& FilePath, const FString& Code, FString& ResultMessage);

	bool WriteProtoFile(const FString& FilePath, const FString& ProtoContent, FString& ResultMessage);

	/**
	 * Get absolute dir path of default game module (first)
	 */
	FString GetDefaultModuleDir();
};
