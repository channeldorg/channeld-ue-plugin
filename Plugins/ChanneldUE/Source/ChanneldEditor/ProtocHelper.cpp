#include "ProtocHelper.h"

FString FProtocHelper::GetProtocPath()
{
	return ANSI_TO_TCHAR(PROTOC_PATH);
}

FString FProtocHelper::GetProtobufIncludePath()
{
	return ANSI_TO_TCHAR(PROTOBUF_INCLUDE_PATH);
}

FString FProtocHelper::BuildProtocProcessArguments(const FString& CppOut, const FString& CppOpt, TArray<FString> ImportProtoPaths, TArray<FString> ProtoFilesToGen)
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
