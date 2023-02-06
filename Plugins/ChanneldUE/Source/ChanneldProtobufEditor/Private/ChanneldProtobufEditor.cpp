#include "ChanneldProtobufEditor.h"

#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FChanneldProtobufEditorModule"

void FChanneldProtobufEditorModule::StartupModule()
{
}

void FChanneldProtobufEditorModule::ShutdownModule()
{
}

namespace ChanneldProtobufHelpers
{
	FString GetProtocPath()
	{
		return ANSI_TO_TCHAR(PROTOC_PATH);
	}

	FString GetProtobufIncludePath()
	{
		return ANSI_TO_TCHAR(PROTOBUF_INCLUDE_PATH);
	}

	FString BuildProtocProcessArguments(const FString& CppOut, const FString& CppOpt, TArray<FString> ImportProtoPaths, TArray<FString> ProtoFilesToGen)
	{
		FString ImportProtoPathParams;
		for (FString ProtoPath : ImportProtoPaths)
		{
			ImportProtoPathParams.Append(FString::Printf(TEXT("--proto_path=\"%s\" "), *ProtoPath));
		}

		FString ProtoFilesToGenParams;
		for (FString ProtoFileToGen : ProtoFilesToGen)
		{
			ProtoFilesToGenParams.Append(ProtoFileToGen + " ");
		}
		return FString::Printf(TEXT("--cpp_out=\"%s\" --cpp_opt=%s --proto_path=\"%s\" %s %s"), *CppOut, *CppOpt, *GetProtobufIncludePath(), *ImportProtoPathParams, *ProtoFilesToGenParams);
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FChanneldProtobufEditorModule, ChanneldProtobufEditor)
