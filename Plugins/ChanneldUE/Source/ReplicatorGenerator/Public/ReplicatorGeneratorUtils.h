#pragma once
#include "ReplicatorGeneratorManager.h"

namespace ChanneldReplicatorGeneratorUtils
{
	enum class EFilterRule: uint8
	{
		NeedToGenerateReplicator,
		NeedToGenRepWithoutIgnore,
		HasRepComponent,
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

		TSet<FSoftClassPath> LoadedRepClasses;
		TSet<FSoftClassPath> AllRepClasses;

	private:
		EFilterRule FilterRule;
	};

	REPLICATORGENERATOR_API bool HasReplicatedProperty(const UClass* TargetClass);

	REPLICATORGENERATOR_API bool HasRPC(const UClass* TargetClass);

	REPLICATORGENERATOR_API bool HasReplicatedPropertyOrRPC(const UClass* TargetClass);

	REPLICATORGENERATOR_API bool HasRepComponent(const UClass* TargetClass);

	REPLICATORGENERATOR_API TArray<const UClass*> GetComponentClasses(const UClass* TargetClass);
	
	REPLICATORGENERATOR_API bool NeedToGenerateReplicator(const UClass* TargetClass, bool bCheckIgnore = true);

	REPLICATORGENERATOR_API bool IsCompilableClassName(const FString& ClassName);

	/**
      * Get absolute dir path of default game module
      */
	REPLICATORGENERATOR_API FString GetDefaultModuleDir();

	REPLICATORGENERATOR_API FString GetUECmdBinary();

}
