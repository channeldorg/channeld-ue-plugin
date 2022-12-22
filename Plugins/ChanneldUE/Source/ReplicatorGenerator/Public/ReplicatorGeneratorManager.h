#pragma once
#include "ReplicatorCodeGenerator.h"
#include "GameFramework/Character.h"
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

public:
	FReplicatorGeneratorManager();
	~FReplicatorGeneratorManager();

	static FReplicatorGeneratorManager& Get();

	TArray<UClass*> IgnoreActorClasses{
		AActor::StaticClass(),
		ACharacter::StaticClass(),
		AController::StaticClass(),
		AGameStateBase::StaticClass(),
		APlayerController::StaticClass(),
		APlayerState::StaticClass(),
	};

	static bool HasReplicatedPropertyOrRPC(UClass* TargetClass);

	TArray<UClass*> GetActorsWithReplicationComp(const TArray<UClass*>& IgnoreActors, const FDateTime& AfterTime);

	bool GeneratedAllReplicators();
	bool GeneratedReplicators(UClass* Target);

	bool WriteCodeFile(const FString& FilePath, const FString& Code, FString& ResultMessage);

	bool WriteProtoFile(const FString& FilePath, const FString& ProtoContent, FString& ResultMessage);

	/**
	 * Get absolute dir path of default game module
	 */
	FString GetDefaultModuleDir();

	FPrevCodeGeneratedInfo LoadPrevCodeGeneratedInfo(const FString& Filename, bool& Success);
	
	void SavePrevCodeGeneratedInfo(const FPrevCodeGeneratedInfo& Info, const FString& Filename, bool& Success);
};
