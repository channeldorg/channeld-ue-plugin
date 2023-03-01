#include "ProtocHelper.h"

FString FProtocHelper::GetProtocPath()
{
	return ANSI_TO_TCHAR(PROTOC_PATH);
}

FString FProtocHelper::GetProtobufIncludePath()
{
	return ANSI_TO_TCHAR(PROTOBUF_INCLUDE_PATH);
}

FString FProtocHelper::BuildProtocProcessArguments(const FString& OutName, const FString& OutValue, const FString& OptName, const FString& OptValue, const TArray<FString>& ImportProtoPaths, const TArray<FString>& ProtoFilesToGen)
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
	return FString::Printf(TEXT("--%s=\"%s\" --%s=%s --proto_path=\"%s\" %s %s"), *OutName, *OutValue, *OptName, *OptValue, *GetProtobufIncludePath(), *ImportProtoPathParams, *ProtoFilesToGenParams);
}

FString FProtocHelper::BuildProtocProcessCppArguments(const FString& CppOut, const FString& CppOpt, const TArray<FString>& ImportProtoPaths, const TArray<FString>& ProtoFilesToGen)
{
	return BuildProtocProcessArguments(TEXT("cpp_out"), CppOut, TEXT("cpp_opt"), CppOpt, ImportProtoPaths, ProtoFilesToGen);
}

FString FProtocHelper::BuildProtocProcessGoArguments(const FString& GoOut, const FString& GoOpt, const TArray<FString>& ImportProtoPaths, const TArray<FString>& ProtoFilesToGen)
{
	return BuildProtocProcessArguments(TEXT("go_out"), GoOut, TEXT("go_opt"), GoOpt, ImportProtoPaths, ProtoFilesToGen);
}
