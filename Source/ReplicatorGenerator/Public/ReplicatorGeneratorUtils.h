#pragma once

#include "ReplicatorGeneratorManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"

namespace ChanneldReplicatorGeneratorUtils
{
	class IExternalTargetToGenerateChanneldDataFieldCallback
	{
	public:
		virtual ~IExternalTargetToGenerateChanneldDataFieldCallback() {}
		virtual bool ShouldGenerate(const UClass* TargetClass) const = 0;
	};

	extern TArray<TUniquePtr<IExternalTargetToGenerateChanneldDataFieldCallback>> ExternalTargetToGenerateChannelDataFieldList;

	enum class EFilterRule: uint8
	{
		NeedToGenerateReplicator,
		HasRepComponent,
		Replication,
	};

	class FReplicationActorFilter : public FUObjectArray::FUObjectCreateListener
	{
	public:
		FReplicationActorFilter(EFilterRule InFilterRule)
			: FilterRule(InFilterRule)
		{
		}

		void StartListen();

		void StopListen();

		virtual void NotifyUObjectCreated(const UObjectBase* Object, int32 Index) override;

		virtual void OnUObjectArrayShutdown() override;

		TSet<FSoftClassPath> FilteredClasses;
		TSet<FSoftClassPath> AllLoadedClasses;

	private:
		EFilterRule FilterRule;
	};

	TArray<const UClass*> ChanneldUEBuiltinClasses{
		AActor::StaticClass(),
		ACharacter::StaticClass(),
		AController::StaticClass(),
		AGameStateBase::StaticClass(),
		APawn::StaticClass(),
		APlayerController::StaticClass(),
		APlayerState::StaticClass(),
		UActorComponent::StaticClass(),
		USceneComponent::StaticClass(),
		UStaticMeshComponent::StaticClass(),
	};

	TSet<const UClass*> ChanneldUEBuiltinClassSet{ChanneldUEBuiltinClasses};

	REPLICATORGENERATOR_API void AddTargetToGenerateChannelDataFieldCallback(IExternalTargetToGenerateChanneldDataFieldCallback* CB);

	REPLICATORGENERATOR_API TArray<const UClass*> GetChanneldUEBuiltinClasses();

	REPLICATORGENERATOR_API bool IsChanneldUEBuiltinClass(const UClass* TargetClass);

	REPLICATORGENERATOR_API bool IsChanneldUEBuiltinSingletonClass(const UClass* TargetClass);

	REPLICATORGENERATOR_API bool HasReplicatedProperty(const UClass* TargetClass);

	REPLICATORGENERATOR_API bool HasRPC(const UClass* TargetClass);

	REPLICATORGENERATOR_API bool HasReplicatedPropertyOrRPC(const UClass* TargetClass);

	REPLICATORGENERATOR_API bool HasTimelineComponent(const UClass* TargetClass);

	REPLICATORGENERATOR_API bool HasRepComponent(const UClass* TargetClass);

	REPLICATORGENERATOR_API TArray<const UClass*> GetComponentClasses(const UClass* TargetClass);

	REPLICATORGENERATOR_API bool TargetToGenerateReplicator(const UClass* TargetClass);

	REPLICATORGENERATOR_API bool TargetToGenerateChannelDataField(const UClass* TargetClass);

	REPLICATORGENERATOR_API bool ContainsUncompilableChar(const FString& Test);

	REPLICATORGENERATOR_API bool IsCompilableClassName(const FString& ClassName);

	REPLICATORGENERATOR_API FString ReplaceUncompilableChar(const FString& String, const FString& ReplaceTo);

	/**
      * Get absolute dir path of default game module
      */
	REPLICATORGENERATOR_API FString GetDefaultModuleDir();

	REPLICATORGENERATOR_API FString GetUECmdBinary();

	REPLICATORGENERATOR_API FString GetHashString(const FString& Target);

	REPLICATORGENERATOR_API void EnsureRepGenIntermediateDir();
}
