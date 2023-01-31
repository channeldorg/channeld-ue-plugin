#pragma once
#include "ReplicatorGeneratorManager.h"

namespace ChanneldReplicatorGeneratorUtils
{
	struct FObjectLoadedListener : public FUObjectArray::FUObjectCreateListener
	{
		void StartListen();

		void StopListen();

		virtual void NotifyUObjectCreated(const UObjectBase* Object, int32 Index) override;

		virtual void OnUObjectArrayShutdown() override;

		TSet<FSoftClassPath> LoadedRepClasses;
	};

	REPLICATORGENERATOR_API bool HasReplicatedProperty(UClass* TargetClass);

	REPLICATORGENERATOR_API bool HasRPC(UClass* TargetClass);

	REPLICATORGENERATOR_API bool HasReplicatedPropertyOrRPC(UClass* TargetClass);

	REPLICATORGENERATOR_API bool HasRepComponent(UClass* TargetClass);

	REPLICATORGENERATOR_API bool NeedToGenerateReplicator(UClass* TargetClass);

	REPLICATORGENERATOR_API bool IsCompilableClassName(const FString& ClassName);

	/**
      * Get absolute dir path of default game module
      */
	REPLICATORGENERATOR_API FString GetDefaultModuleDir();

	REPLICATORGENERATOR_API FString GetUECmdBinary();

}
