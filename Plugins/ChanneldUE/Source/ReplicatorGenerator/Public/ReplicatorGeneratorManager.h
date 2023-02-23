#pragma once
#include "AddCompToBPSubsystem.h"
#include "ReplicatorCodeGenerator.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

struct FPrevCodeGeneratedInfo
{
	FDateTime GeneratedTime;
};

class REPLICATORGENERATOR_API FReplicatorGeneratorManager
{
protected:
	FReplicatorCodeGenerator* CodeGenerator;

	FString ReplicatorStorageDir;

public:

	FReplicatorGeneratorManager();
	~FReplicatorGeneratorManager();

	/**
	 * Get the singleton instance of FReplicatorGeneratorManager.
	 */
	static FReplicatorGeneratorManager& Get();

	/**
	 * Get the directory of the generated replicators.
	 */
	FString GetReplicatorStorageDir();

	/**
	 * Header files of the target class can be found or not.
	 *
	 * @param TargetClass The target class.
	 * @return true if the header files of the target class can be found.
	 */
	bool HeaderFilesCanBeFound(UClass* TargetClass);

	/**
	 * Is the target class ignored or not. If the target class is ignored, we will not generate replicator for it.
	 *
	 * @param TargetClass The target class.
	 * @return true if the target class is ignored.
	 */
	bool IsIgnoredActor(UClass* TargetClass);

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
	 * @param Targets The target actors.
	 * @param GetGoPackage The function to get the 'go_package' for proto file.
	 * @return true if the replicators are generated successfully.
	 */
	bool GeneratedReplicators(TArray<UClass*> Targets, const TFunction<FString(const FString& PackageName)>* GetGoPackage = nullptr);

	/**
	 * Write the given code to the disk.
	 *
	 * @param FilePath The path of the file to write.
	 * @param Code The code to write.
	 * @param ResultMessage The result message.
	 * @return true if the code is written successfully.
	 */
	bool WriteCodeFile(const FString& FilePath, const FString& Code, FString& ResultMessage);

	/**
	 * Write the given proto definitions to the disk.
	 *
	 * @param FilePath The path of the file to write.
	 * @param ProtoContent The proto definitions to write.
	 * @param ResultMessage The result message.
	 * @return true if the proto definitions are written successfully.
	 */
	bool WriteProtoFile(const FString& FilePath, const FString& ProtoContent, FString& ResultMessage);

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
	 * Remove all the generated replicator files from 'GeneratedReplicators' directory.
	 */
	void RemoveGeneratedCode();

	FPrevCodeGeneratedInfo LoadPrevCodeGeneratedInfo(const FString& Filename, bool& Success);

	void SavePrevCodeGeneratedInfo(const FPrevCodeGeneratedInfo& Info, const FString& Filename, bool& Success);

private:
	
	TSet<UClass*> IgnoreActorClasses{
		AActor::StaticClass(),
		ACharacter::StaticClass(),
		AController::StaticClass(),
		AGameStateBase::StaticClass(),
		AGameState::StaticClass(),
		APlayerController::StaticClass(),
		APlayerState::StaticClass(),
	};

	TSet<FString> IgnoreActorClassPaths{
		TEXT("/Script/Engine.SkyLight"),
		TEXT("/Script/Engine.WorldSettings"),
		TEXT("/Script/Engine.ExponentialHeightFog"),
		TEXT("/Script/Engine.Emitter"),
		TEXT("/Script/Engine.SkeletalMeshActor"),
	};
};
