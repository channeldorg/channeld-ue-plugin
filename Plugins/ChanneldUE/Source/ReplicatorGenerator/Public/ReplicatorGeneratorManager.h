#pragma once
#include "ReplicatorCodeGenerator.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

static 

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

	TArray<UClass*> GetActorsWithReplicationComp(TArray<UClass*> IgnoreActors);

	bool GeneratedAllReplicators();
	bool GeneratedReplicators(UClass* Target);

	bool WriteCodeFile(const FString& FilePath, const FString& Code, FString& ResultMessage);

	bool WriteProtoFile(const FString& FilePath, const FString& ProtoContent, FString& ResultMessage);

	/**
	 * Get absolute dir path of default game module
	 */
	FString GetDefaultModuleDir();
};
