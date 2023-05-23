#include "ReplicatorGeneratorManager.h"

#include "ReplicatorGeneratorUtils.h"
#include "Engine/AssetManager.h"
#include "Engine/SCS_Node.h"
#include "GameFramework/Character.h"
#include "HAL/FileManagerGeneric.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Persistence/ChannelDataSchemaController.h"

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

FString FReplicatorGeneratorManager::GetDefaultModuleName()
{
	return FPaths::GetBaseFilename(GetDefaultModuleDir());
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

FString FReplicatorGeneratorManager::GetDefaultProtoPackageName() const
{
	return GenManager_DefaultProtoPackageName;
}

bool FReplicatorGeneratorManager::HeaderFilesCanBeFound(const UClass* TargetClass)
{
	const FString TargetHeadFilePath = CodeGenerator->GetClassHeadFilePath(TargetClass->GetPrefixCPP() + TargetClass->GetName());
	return !TargetHeadFilePath.IsEmpty();
}

bool FReplicatorGeneratorManager::GenerateReplication(const FString GoPackageImportPathPrefix, bool CompatibleRecompilation)
{
	TArray<FChannelDataInfo> ChannelDataInfos;

	UChannelDataSchemaController* ChannelDataSchemaController = GEditor->GetEditorSubsystem<UChannelDataSchemaController>();
	TArray<FChannelDataSchema> ChannelDataSchemata;
	ChannelDataSchemaController->GetChannelDataSchemata(ChannelDataSchemata);
	for (const FChannelDataSchema& ChannelDataSchema : ChannelDataSchemata)
	{
		if (ChannelDataSchema.StateSchemata.Num() == 0)
		{
			continue;
		}
		ChannelDataInfos.Add(ChannelDataSchema);
	}

	// We need to include the header file of the target class in 'ChanneldReplicatorRegister.h'. so we need to know the include path of the target class from 'uhtmanifest' file.
	// But the 'uhtmanifest' file is a large json file, so we need to read and parser it only once.
	CodeGenerator->RefreshModuleInfoByClassName();

	FGeneratedCodeBundle ReplicatorCodeBundle;
	const FString ProtoPackageName = GetDefaultProtoPackageName();
	const FString GoPackageImportPath = GoPackageImportPathPrefix / ProtoPackageName;
	CodeGenerator->Generate(
		ChannelDataInfos
		, ProtoPackageName
		, CompatibleRecompilation ? ChanneldReplicatorGeneratorUtils::GetHashString(FDateTime::Now().ToString()) : TEXT("")
		, GoPackageImportPath
		, ReplicatorCodeBundle
	);
	FString Message;

	// Generate type definitions file
	WriteCodeFile(GetReplicatorStorageDir() / GenManager_TypeDefinitionHeadFile, ReplicatorCodeBundle.TypeDefinitionsHeadCode, Message);
	WriteCodeFile(GetReplicatorStorageDir() / GenManager_TypeDefinitionCppFile, ReplicatorCodeBundle.TypeDefinitionsCppCode, Message);

	// Generate replicator code file
	for (FReplicatorCode& ReplicatorCode : ReplicatorCodeBundle.ReplicatorCodes)
	{
		WriteCodeFile(GetReplicatorStorageDir() / ReplicatorCode.HeadFileName, ReplicatorCode.HeadCode, Message);
		WriteCodeFile(GetReplicatorStorageDir() / ReplicatorCode.CppFileName, ReplicatorCode.CppCode, Message);
		WriteProtoFile(GetReplicatorStorageDir() / ReplicatorCode.ProtoFileName, ReplicatorCode.ProtoDefinitionsFile, Message);
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
	WriteCodeFile(GetReplicatorStorageDir() / GenManager_RepRegistrationHeadFile, ReplicatorCodeBundle.ReplicatorRegistrationHeadCode, Message);

	// Generate global struct declarations file and proto definitions file
	WriteCodeFile(GetReplicatorStorageDir() / GenManager_GlobalStructHeaderFile, ReplicatorCodeBundle.GlobalStructCodes, Message);
	WriteProtoFile(GetReplicatorStorageDir() / GenManager_GlobalStructProtoFile, ReplicatorCodeBundle.GlobalStructProtoDefinitions, Message);

	TMap<EChanneldChannelType, FString> ChannelTypeToChannelDataMsgMap;
	for (const FChannelDataCode& ChannelDataCode : ReplicatorCodeBundle.ChannelDataCodes)
	{
		ChannelTypeToChannelDataMsgMap.Add(ChannelDataCode.ChannelType, ChannelDataCode.ChannelDataMsgName);
		WriteCodeFile(GetReplicatorStorageDir() / ChannelDataCode.ProcessorHeadFileName, ChannelDataCode.ProcessorHeadCode, Message);
		WriteProtoFile(GetReplicatorStorageDir() / ChannelDataCode.ProtoFileName, ChannelDataCode.ProtoDefsFile, Message);
	}
	// Generate channel data golang merge code temporary file.
	ChanneldReplicatorGeneratorUtils::EnsureRepGenIntermediateDir();
	WriteCodeFile(GenManager_TemporaryGoMergeCodePath, ReplicatorCodeBundle.ChannelDataMerge_GoCode, Message);
	WriteCodeFile(GenManager_TemporaryGoRegistrationCodePath, ReplicatorCodeBundle.ChannelDataRegistration_GoCode, Message);

	// Save the generated manifest file
	FGeneratedManifest Manifest(
		FDateTime::UtcNow()
		, ProtoPackageName
		, GenManager_TemporaryGoMergeCodePath
		, GenManager_TemporaryGoRegistrationCodePath
		, ChannelTypeToChannelDataMsgMap
	);

	if (!SaveGeneratedManifest(Manifest))
	{
		UE_LOG(LogChanneldRepGenerator, Error, TEXT("Failed to save the generated manifest file"));
		return false;
	}

	return true;
}

inline bool FReplicatorGeneratorManager::WriteCodeFile(const FString& FilePath, const FString& Code, FString& ResultMessage)
{
	const FString Prompt = TEXT("// Generated by ChanneldUE's replication code generator. DO NOT EDIT.\n");
	bool bSuccess = FFileHelper::SaveStringToFile(Prompt + Code, *FilePath);
	return bSuccess;
}

inline bool FReplicatorGeneratorManager::WriteProtoFile(const FString& FilePath, const FString& ProtoContent, FString& ResultMessage)
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

void FReplicatorGeneratorManager::RemoveGeneratedCodeFiles()
{
	TArray<FString> AllCodeFiles;
	IFileManager::Get().FindFiles(AllCodeFiles, *GetReplicatorStorageDir());
	for (const FString& FilePath : AllCodeFiles)
	{
		IFileManager::Get().Delete(*(GetReplicatorStorageDir() / FilePath));
	}
}

inline FString FReplicatorGeneratorManager::GetTemporaryGoProtoDataFilePath() const
{
	return GenManager_TemporaryGoMergeCodePath;
}

inline bool FReplicatorGeneratorManager::WriteTemporaryGoProtoData(const FString& Code, FString& ResultMessage)
{
	ChanneldReplicatorGeneratorUtils::EnsureRepGenIntermediateDir();
	return WriteCodeFile(GetTemporaryGoProtoDataFilePath(), Code, ResultMessage);
}

bool FReplicatorGeneratorManager::LoadLatestGeneratedManifest(FGeneratedManifest& Result)
{
	return GeneratedManifestModel.GetData(Result, true);
}

bool FReplicatorGeneratorManager::SaveGeneratedManifest(const FGeneratedManifest& Manifest)
{
	ChanneldReplicatorGeneratorUtils::EnsureRepGenIntermediateDir();
	return GeneratedManifestModel.SaveData(Manifest);
}
