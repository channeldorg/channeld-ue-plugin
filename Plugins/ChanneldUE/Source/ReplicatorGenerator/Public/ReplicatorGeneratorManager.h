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

	FString DefaultModuleDirPath;

public:
	FReplicatorGeneratorManager();
	~FReplicatorGeneratorManager();

	static FReplicatorGeneratorManager& Get();

	bool HeaderFilesCanBeFound(UClass* TargetClass);

	bool IsIgnoredActor(UClass* TargetClass);

	void StartGenerateReplicator();
	void StopGenerateReplicator();

	bool GeneratedReplicators(TArray<UClass*> Targets);

	bool WriteCodeFile(const FString& FilePath, const FString& Code, FString& ResultMessage);

	bool WriteProtoFile(const FString& FilePath, const FString& ProtoContent, FString& ResultMessage);

	/**
	 * Get absolute dir path of default game module
	 */
	FString GetDefaultModuleDir();

	TArray<FString> GetGeneratedTargetClass();

	void RemoveGeneratedReplicator(const FString& ClassName);

	void RemoveGeneratedReplicators(const TArray<FString>& ClassNames);

	void RemoveGeneratedCode();

	FPrevCodeGeneratedInfo LoadPrevCodeGeneratedInfo(const FString& Filename, bool& Success);

	void SavePrevCodeGeneratedInfo(const FPrevCodeGeneratedInfo& Info, const FString& Filename, bool& Success);

	TArray<FString> GetAllGeneratedProtoFilePath();

	void AddComponentToActorBlueprint(UClass* CompClass, FName CompName);

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
