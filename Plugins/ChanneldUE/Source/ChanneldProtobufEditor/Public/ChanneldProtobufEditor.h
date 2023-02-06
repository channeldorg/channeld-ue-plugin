#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FChanneldProtobufEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

namespace ChanneldProtobufHelpers
{
	CHANNELDPROTOBUFEDITOR_API FString GetProtocPath();

	CHANNELDPROTOBUFEDITOR_API FString GetProtobufIncludePath();

	CHANNELDPROTOBUFEDITOR_API FString BuildProtocProcessArguments(const FString& CppOut, const FString& CppOpt, TArray<FString> ImportProtoPaths, TArray<FString> ProtoFilesToGen);
}
