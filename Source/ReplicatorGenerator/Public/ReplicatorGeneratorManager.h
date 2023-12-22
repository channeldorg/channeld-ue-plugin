#pragma once
#include "AddCompToBPSubsystem.h"
#include "ReplicatorCodeGenerator.h"
#include "ReplicatorGeneratorDefinition.h"
#include "GameFramework/GameStateBase.h"
#include "Persistence/JsonModel.h"
#include "ReplicatorGeneratorManager.generated.h"

/**
 * Persistent info about latest generated codes.
 */
USTRUCT(BlueprintType)
struct REPLICATORGENERATOR_API FGeneratedManifest
{
	GENERATED_BODY()

	UPROPERTY()
	FDateTime GeneratedTime;

	UPROPERTY()
	FString ProtoPackageName;

	UPROPERTY()
	FString TemporaryGoMergeCodePath;

	UPROPERTY()
	FString TemporaryGoRegistrationCodePath;

	// TODO: FString -> TMap<EChanneldChannelType, FString>

	UPROPERTY()
	TMap<EChanneldChannelType, FString> ChannelDataMsgNames;

	FGeneratedManifest() = default;

	FGeneratedManifest(
		const FDateTime& InGeneratedTime
		, const FString& InProtoPackageName
		, const FString& InTemporaryGoMergeCodePath
		, const FString& InTemporaryGoRegistrationCodePath
		, const TMap<EChanneldChannelType, FString>& InChannelDataMsgNames
	)
		: GeneratedTime(InGeneratedTime)
		  , ProtoPackageName(InProtoPackageName)
		  , TemporaryGoMergeCodePath(InTemporaryGoMergeCodePath)
		  , TemporaryGoRegistrationCodePath(InTemporaryGoRegistrationCodePath)
		  , ChannelDataMsgNames(InChannelDataMsgNames)
	{
	}
};

class REPLICATORGENERATOR_API FReplicatorGeneratorManager
{
protected:
	FReplicatorCodeGenerator* CodeGenerator;
	TJsonModel<FGeneratedManifest> GeneratedManifestModel = GenManager_GeneratedManifestFilePath;

	FString DefaultModuleDir;
	FString ReplicatorStorageDir;

public:
	FReplicatorGeneratorManager();
	~FReplicatorGeneratorManager();

	/**
	 * Get the singleton instance of FReplicatorGeneratorManager.
	 */
	static FReplicatorGeneratorManager& Get();

	/*
	 * Get the directory of the default game module.
	 */
	FString GetDefaultModuleDir();

	/*
	 * Get the name of the default game module.
	 */
	FString GetDefaultModuleName();

	/**
	 * Get the directory of the generated replicators.
	 */
	FString GetReplicatorStorageDir();

	/**
	 * Get the default proto package name. The name is used for the proto package name and proto cpp namespace.
	 */
	FString GetDefaultProtoPackageName() const;

	/**
	 * Header files of the target class can be found or not.
	 *
	 * @param TargetClass The target class.
	 * @return true if the header files of the target class can be found.
	 */
	bool HeaderFilesCanBeFound(const UClass* TargetClass);

	/**
	 * Generate replicators for the replication actors from registry table.
	 *
	 * @param GoPackageImportPathPrefix If the go package is "github.com/metaworking/channeld/examples/channeld-ue-tps/tpspb", the prefix is "github.com/metaworking/channeld/examples/channeld-ue-tps".
	 * @param CompatibleRecompilation If true, the generated code will be compatible with the previous generated code.
	 * @return true if the replicators are generated successfully.
	 */
	bool GenerateReplication(const FString GoPackageImportPathPrefix, bool CompatibleRecompilation);

	/**
	 * Write the given code to the disk.
	 *
	 * @param FilePath The path of the file to write.
	 * @param Code The code to write.
	 * @param ResultMessage The result message.
	 * @return true if the code is written successfully.
	 */
	inline bool WriteCodeFile(const FString& FilePath, const FString& Code, FString& ResultMessage);

	/**
	 * Write the given proto definitions to the disk.
	 *
	 * @param FilePath The path of the file to write.
	 * @param ProtoContent The proto definitions to write.
	 * @param ResultMessage The result message.
	 * @return true if the proto definitions are written successfully.
	 */
	inline bool WriteProtoFile(const FString& FilePath, const FString& ProtoContent, FString& ResultMessage);

	/**
	 * Get the generated replicator classes in 'GeneratedReplicators' directory.
	 *
	 * @return List of the generated replicator classes.
	 */
	TArray<FString> GetGeneratedTargetClasses();

	/**
	 * Get the generated proto files in 'GeneratedReplicators' directory.
	 *
	 * return List of the generated proto file path relative to 'GeneratedReplicators' directory.
	 */
	TArray<FString> GetGeneratedProtoFiles();

	/**
	 * Remove the generated replicator files from 'GeneratedReplicators' directory, including .h, .cpp, .proto, .pb.h, .pb.cpp files.
	 *
	 * @param ClassName The name of target of replicator.
	 */
	void RemoveGeneratedReplicator(const FString& ClassName);

	/**
	 * Remove the generated replicator files from 'GeneratedReplicators' directory, including .h, .cpp, .proto, .pb.h, .pb.cpp files.
	 *
	 * @param ClassNames The names of targets of replicators.
	 */
	void RemoveGeneratedReplicators(const TArray<FString>& ClassNames);

	/**
	 * Remove all the generated files from 'GeneratedReplicators' directory.
	 */
	void RemoveGeneratedCodeFiles();

	inline FString GetTemporaryGoProtoDataFilePath() const;
	inline bool WriteTemporaryGoProtoData(const FString& Code, FString& ResultMessage);

	bool LoadLatestGeneratedManifest(FGeneratedManifest& Result);

	bool SaveGeneratedManifest(const FGeneratedManifest& Manifest);

	const FModuleInfo* GetModuleInfo(const FString& ClassName) const;

	TSet<FString> DefaultSkipGenRep = {
		TEXT("/Script/Engine.WorldSettings"),
		TEXT("/Script/Engine.Light"),
		TEXT("/Script/Engine.SkyLight"),
		TEXT("/Script/Engine.StaticMeshComponent"),
	};
};
