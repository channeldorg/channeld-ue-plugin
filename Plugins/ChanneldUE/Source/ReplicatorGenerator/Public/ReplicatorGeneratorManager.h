#pragma once
#include "AddCompToBPSubsystem.h"
#include "ReplicatorCodeGenerator.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

/**
 * Persistent info about latest generated codes.
 */
struct REPLICATORGENERATOR_API FGeneratedManifest
{
	FDateTime GeneratedTime;
	FString ProtoPackageName;
	FString TemporaryGoMergeCodePath;
	FString TemporaryGoRegistrationCodePath;
	// TODO: FString -> TMap<EChanneldChannelType, FString>
	FString ChannelDataMsgName;
};

class REPLICATORGENERATOR_API FReplicatorGeneratorManager
{
protected:
	FReplicatorCodeGenerator* CodeGenerator;

	FString DefaultModuleDir;
	FString ReplicatorStorageDir;

public:
	FReplicatorGeneratorManager();
	~FReplicatorGeneratorManager();

	/**
	 * Get the singleton instance of FReplicatorGeneratorManager.
	 */
	static FReplicatorGeneratorManager& Get();

	FString GetDefaultModuleDir();

	/**
	 * Get the directory of the generated replicators.
	 */
	FString GetReplicatorStorageDir();


	/**
	 * Get the default proto package name. The name is used for the proto package name and proto cpp namespace.
	 */
	FString GetDefaultProtoPackageName() const;

	/*
	 * Get the name of the header file of the ChannelDataProcessor.
	 */
	FString GetDefaultModuleName();

	/**
	 * Header files of the target class can be found or not.
	 *
	 * @param TargetClass The target class.
	 * @return true if the header files of the target class can be found.
	 */
	bool HeaderFilesCanBeFound(const UClass* TargetClass);

	/**
	 * Is the target class ignored or not. If the target class is ignored, we will not generate replicator for it.
	 *
	 * @param TargetClass The target class.
	 * @return true if the target class is ignored.
	 */
	bool IsIgnoredActor(const UClass* TargetClass);

	/**
	 * Invoke this function before generating replicators.
	 * We need to include the header file of the target class in 'ChanneldReplicatorRegister.h'. so we need to know the include path of the target class from 'uhtmanifest' file.
	 * But the 'uhtmanifest' file is a large json file, so we need to read and parser it only once.
	 */
	void StartGenerateReplicator();
	void StopGenerateReplicator();

	/**
	 * Generate replicators for the given target actors.
	 *
	 * @param TargetClasses The target actors.
	 * @param GoPackageImportPathPrefix If the go package is "channeld.clewcat.com/channeld/examples/channeld-ue-tps/tpspb", the prefix is "channeld.clewcat.com/channeld/examples/channeld-ue-tps".
	 * @return true if the replicators are generated successfully.
	 */
	bool GeneratedReplicators(const TArray<const UClass*>& TargetClasses, const FString GoPackageImportPathPrefix);

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

	inline void EnsureReplicatorGeneratedIntermediateDir();

	inline FString GetTemporaryGoProtoDataFilePath() const;
	inline bool WriteTemporaryGoProtoData(const FString& Code, FString& ResultMessage);

	bool LoadLatestGeneratedManifest(FGeneratedManifest& Result, FString& Message) const;
	bool LoadLatestGeneratedManifest(const FString& Filename, FGeneratedManifest& Result, FString& Message) const;

	bool SaveGeneratedManifest(const FGeneratedManifest& Manifest, FString& Message);
	bool SaveGeneratedManifest(const FGeneratedManifest& Manifest, const FString& Filename, FString& Message);

private:
	TSet<UClass*> IgnoreActorClasses{
		AActor::StaticClass(),
		ACharacter::StaticClass(),
		AController::StaticClass(),
		AGameStateBase::StaticClass(),
		APawn::StaticClass(),
		APlayerController::StaticClass(),
		APlayerState::StaticClass(),
		UActorComponent::StaticClass(),
		USceneComponent::StaticClass(),
		UCharacterMovementComponent::StaticClass(),
	};

	TSet<FString> IgnoreActorClassPaths{
		TEXT("/Script/Engine.SkyLight"),
		TEXT("/Script/Engine.WorldSettings"),
		TEXT("/Script/Engine.ExponentialHeightFog"),
		TEXT("/Script/Engine.Emitter"),
		TEXT("/Script/Engine.SkeletalMeshActor"),
	};
};
