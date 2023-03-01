#pragma once

class CHANNELDEDITOR_API FProtocHelper
{
public:
	static FString GetProtocPath();

	static FString GetProtobufIncludePath();

	static FString BuildProtocProcessArguments(const FString& OutName, const FString& OutValue, const FString& OptName, const FString& OptValue, const TArray<FString>& ImportProtoPaths, const TArray<FString>& ProtoFilesToGen);
	static FString BuildProtocProcessCppArguments(const FString& CppOut, const FString& CppOpt, const TArray<FString>& ImportProtoPaths, const TArray<FString>& ProtoFilesToGen);
	static FString BuildProtocProcessGoArguments(const FString& GoOut, const FString& GoOpt, const TArray<FString>& ImportProtoPaths, const TArray<FString>& ProtoFilesToGen);
};
