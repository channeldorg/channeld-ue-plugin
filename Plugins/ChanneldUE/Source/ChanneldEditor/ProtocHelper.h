#pragma once

class CHANNELDEDITOR_API FProtocHelper
{
public:
	static FString GetProtocPath();

	static FString GetProtobufIncludePath();

	static FString BuildProtocProcessArguments(const FString& CppOut, const FString& CppOpt, TArray<FString> ImportProtoPaths, TArray<FString> ProtoFilesToGen);
};
