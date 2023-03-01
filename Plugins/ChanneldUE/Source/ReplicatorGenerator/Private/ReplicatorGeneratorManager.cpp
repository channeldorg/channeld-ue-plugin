#include "ReplicatorGeneratorManager.h"

#include "ReplicatorGeneratorDefinition.h"
#include "ReplicatorGeneratorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "Engine/SCS_Node.h"
#include "GameFramework/Character.h"
#include "HAL/FileManagerGeneric.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Replication/ChanneldReplicationComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY(LogChanneldRepGenerator);

FReplicatorGeneratorManager::FReplicatorGeneratorManager()
{
	CodeGenerator = new FReplicatorCodeGenerator();
}

FReplicatorGeneratorManager::~FReplicatorGeneratorManager()
{
	if (CodeGenerator != nullptr)
	{
		delete CodeGenerator;
		CodeGenerator = nullptr;
	}
}

FReplicatorGeneratorManager& FReplicatorGeneratorManager::Get()
{
	static FReplicatorGeneratorManager Singleton;
	return Singleton;
}

FString FReplicatorGeneratorManager::GetDefaultModuleDir()
{
	if (DefaultModuleDir.IsEmpty())
	{
		DefaultModuleDir = ChanneldReplicatorGeneratorUtils::GetDefaultModuleDir();
		FPaths::NormalizeDirectoryName(DefaultModuleDir);
	}
	return DefaultModuleDir;
}

FString FReplicatorGeneratorManager::GetReplicatorStorageDir()
{
	if (ReplicatorStorageDir.IsEmpty())
	{
		ReplicatorStorageDir = GetDefaultModuleDir() / GenManager_GeneratedCodeDir;
		FPaths::NormalizeDirectoryName(ReplicatorStorageDir);
	}
	return ReplicatorStorageDir;
}

FString FReplicatorGeneratorManager::GetDefaultProtoPackageName()
{
	return DefaultProtoPackageName = FPaths::GetBaseFilename(GetDefaultModuleDir()).ToLower() + TEXT("pb");
}

FString FReplicatorGeneratorManager::GetDefaultModuleName()
{
	return FPaths::GetBaseFilename(GetDefaultModuleDir());
}

bool FReplicatorGeneratorManager::HeaderFilesCanBeFound(const UClass* TargetClass)
{
	const FString TargetHeadFilePath = CodeGenerator->GetClassHeadFilePath(TargetClass->GetPrefixCPP() + TargetClass->GetName());
	return !TargetHeadFilePath.IsEmpty();
}

bool FReplicatorGeneratorManager::IsIgnoredActor(const UClass* TargetClass)
{
	return IgnoreActorClasses.Contains(TargetClass) || IgnoreActorClassPaths.Contains(TargetClass->GetPathName());
}

void FReplicatorGeneratorManager::StartGenerateReplicator()
{
	CodeGenerator->RefreshModuleInfoByClassName();
}

void FReplicatorGeneratorManager::StopGenerateReplicator()
{
}

bool FReplicatorGeneratorManager::GeneratedReplicators(const TArray<const UClass*>& TargetClasses, const FString GoPackageImportPathPrefix)
{
	UE_LOG(LogChanneldRepGenerator, Display, TEXT("Start generating %d replicators"), TargetClasses.Num());

	TArray<FString> IncludeActorCodes, RegisterReplicatorCodes;

	FGeneratedCodeBundle ReplicatorCodeBundle;

	const FString ProtoPackageName = GetDefaultProtoPackageName();
	const FString GoPackageImportPath = GoPackageImportPathPrefix + ProtoPackageName;
	CodeGenerator->Generate(
		TargetClasses,
		GetDefaultModuleDir(),
		ProtoPackageName,
		(GoPackageImportPathPrefix + ProtoPackageName),
		ReplicatorCodeBundle
	);
	FString WriteCodeFileMessage;

	// Generate type definitions file
	WriteCodeFile(GetReplicatorStorageDir() / GenManager_TypeDefinitionHeadFile, ReplicatorCodeBundle.TypeDefinitionsHeadCode, WriteCodeFileMessage);
	WriteCodeFile(GetReplicatorStorageDir() / GenManager_TypeDefinitionCppFile, ReplicatorCodeBundle.TypeDefinitionsCppCode, WriteCodeFileMessage);

	// Generate replicator code file
	for (FReplicatorCode& ReplicatorCode : ReplicatorCodeBundle.ReplicatorCodes)
	{
		WriteCodeFile(GetReplicatorStorageDir() / ReplicatorCode.HeadFileName, ReplicatorCode.HeadCode, WriteCodeFileMessage);
		WriteCodeFile(GetReplicatorStorageDir() / ReplicatorCode.CppFileName, ReplicatorCode.CppCode, WriteCodeFileMessage);
		WriteProtoFile(GetReplicatorStorageDir() / ReplicatorCode.ProtoFileName, ReplicatorCode.ProtoDefinitionsFile, WriteCodeFileMessage);
		UE_LOG(
			LogChanneldRepGenerator,
			Verbose,
			TEXT("The replicator for the target class [%s] was generated successfully.\n    Package path: %s\n    Head file: %s\n    CPP file: %s\n    Proto file: %s\n"),
			*ReplicatorCode.ActorDecorator->GetOriginActorName(),
			*ReplicatorCode.ActorDecorator->GetPackagePathName(),
			*ReplicatorCode.HeadFileName,
			*ReplicatorCode.CppFileName,
			*ReplicatorCode.ProtoFileName
		);
	}
	// Generate replicator registration code file
	WriteCodeFile(GetReplicatorStorageDir() / GenManager_RepRegistrationHeadFile, ReplicatorCodeBundle.ReplicatorRegistrationHeadCode, WriteCodeFileMessage);

	// Generate global struct declarations file and proto definitions file
	WriteCodeFile(GetReplicatorStorageDir() / GenManager_GlobalStructHeaderFile, ReplicatorCodeBundle.GlobalStructCodes, WriteCodeFileMessage);
	WriteProtoFile(GetReplicatorStorageDir() / GenManager_GlobalStructProtoFile, ReplicatorCodeBundle.GlobalStructProtoDefinitions, WriteCodeFileMessage);

	// Generate channel data processor code file
	const FString DefaultModuleName = GetDefaultModuleName();
	WriteCodeFile(GetReplicatorStorageDir() / TEXT("ChannelData_") + DefaultModuleName + CodeGen_HeadFileExtension, ReplicatorCodeBundle.ChannelDataProcessorHeadCode, WriteCodeFileMessage);
	WriteProtoFile(GetReplicatorStorageDir() / TEXT("ChannelData_") + DefaultModuleName + CodeGen_ProtoFileExtension, ReplicatorCodeBundle.ChannelDataProtoDefsFile, WriteCodeFileMessage);

	UE_LOG(
		LogChanneldRepGenerator,
		Display,
		TEXT("The generation of replicators is completed, %d replicators need to be generated, a total of %d replicators are generated"),
		TargetClasses.Num(), ReplicatorCodeBundle.ReplicatorCodes.Num()
	);

	return true;
}

bool FReplicatorGeneratorManager::WriteCodeFile(const FString& FilePath, const FString& Code, FString& ResultMessage)
{
	bool bSuccess = FFileHelper::SaveStringToFile(Code, *FilePath);
	return bSuccess;
}

bool FReplicatorGeneratorManager::WriteProtoFile(const FString& FilePath, const FString& ProtoContent, FString& ResultMessage)
{
	return WriteCodeFile(FilePath, ProtoContent, ResultMessage);
}

TArray<FString> FReplicatorGeneratorManager::GetGeneratedTargetClasses()
{
	TArray<FString> HeadFiles;
	IFileManager::Get().FindFiles(HeadFiles, *GetReplicatorStorageDir(), *CodeGen_HeadFileExtension);

	TArray<FString> Result;
	for (const FString& HeadFile : HeadFiles)
	{
		FRegexPattern MatherPatter(FString(TEXT(R"EOF(^Channeld(\w+)Replicator\.h$)EOF")));
		FRegexMatcher Matcher(MatherPatter, HeadFile);
		if (Matcher.FindNext())
		{
			FString CaptureString = Matcher.GetCaptureGroup(1);
			Result.Add(CaptureString);
		}
	}
	return Result;
}

TArray<FString> FReplicatorGeneratorManager::GetGeneratedProtoFiles()
{
	TArray<FString> AllCodeFiles;
	IFileManager::Get().FindFiles(AllCodeFiles, *GetReplicatorStorageDir(), *CodeGen_ProtoFileExtension);
	return AllCodeFiles;
}

void FReplicatorGeneratorManager::RemoveGeneratedReplicator(const FString& ClassName)
{
	IPlatformFile& FileManager = FPlatformFileManager::Get().GetPlatformFile();
	FileManager.DeleteFile(*FString::Printf(TEXT("Channeld%sReplicator%s"), *ClassName, *CodeGen_CppFileExtension));
	FileManager.DeleteFile(*(FString::Printf(TEXT("Channeld%sReplicator%s"), *ClassName, *CodeGen_HeadFileExtension)));
	FileManager.DeleteFile(*(ClassName + CodeGen_ProtoFileExtension));
	FileManager.DeleteFile(*(ClassName + CodeGen_ProtoPbHeadExtension));
	FileManager.DeleteFile(*(ClassName + CodeGen_ProtoPbCPPExtension));
}

void FReplicatorGeneratorManager::RemoveGeneratedReplicators(const TArray<FString>& ClassNames)
{
	for (const FString& ClassName : ClassNames)
	{
		RemoveGeneratedReplicator(ClassName);
	}
}

void FReplicatorGeneratorManager::RemoveGeneratedCode()
{
	TArray<FString> AllCodeFiles;
	IFileManager::Get().FindFiles(AllCodeFiles, *GetReplicatorStorageDir());
	for (const FString& FilePath : AllCodeFiles)
	{
		IFileManager::Get().Delete(*(GetReplicatorStorageDir() / FilePath));
	}
}

FPrevCodeGeneratedInfo FReplicatorGeneratorManager::LoadPrevCodeGeneratedInfo(const FString& Filename, bool& Success)
{
	FPrevCodeGeneratedInfo Result;
	Result.GeneratedTime = FDateTime(1970, 1, 1);
	Success = false;
	FString Json;

	if (!FFileHelper::LoadFileToString(Json, *Filename))
	{
		UE_LOG(LogChanneldRepGenerator, Error, TEXT("Unable to load PrevCodeGeneratedInfo: %s"), *Filename);
		return Result;
	}

	TSharedPtr<FJsonObject> RootObject = TSharedPtr<FJsonObject>();
	TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Json);

	if (!FJsonSerializer::Deserialize(Reader, RootObject))
	{
		UE_LOG(LogChanneldRepGenerator, Error, TEXT("PrevCodeGeneratedInfo is malformed: %s"), *Filename);
		return Result;
	}
	TSharedPtr<FJsonValue>* JsonValue = RootObject->Values.Find(TEXT("GeneratedTime"));
	if (!JsonValue)
	{
		UE_LOG(LogChanneldRepGenerator, Error, TEXT("Unable to find field 'GeneratedTime'"));
		return Result;
	}
	double GeneratedTime;
	(*JsonValue)->AsArgumentType(GeneratedTime);
	Result.GeneratedTime = FDateTime::FromUnixTimestamp(GeneratedTime);
	Success = true;
	return Result;
}

void FReplicatorGeneratorManager::SavePrevCodeGeneratedInfo(const FPrevCodeGeneratedInfo& Info, const FString& Filename, bool& Success)
{
	Success = false;
	FString Json;
	TSharedPtr<FJsonObject> RootObject = TSharedPtr<FJsonObject>();

	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&Json);
	JsonWriter->WriteObjectStart();
	JsonWriter->WriteValue(TEXT("GeneratedTime"), FString::Printf(TEXT("%lld"), Info.GeneratedTime.ToUnixTimestamp()));
	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();
	if (FFileHelper::SaveStringToFile(Json, *Filename))
	{
		UE_LOG(LogChanneldRepGenerator, Error, TEXT("Unable to save PrevCodeGeneratedInfo: %s"), *Filename);
		return;
	}
	Success = true;
}
