#include "ReplicatorGeneratorManager.h"

#include "ReplicatorGeneratorDefinition.h"
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
	DefaultModuleDirPath = GetDefaultModuleDir() / GenManager_GeneratedCodeDir;
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

bool FReplicatorGeneratorManager::HeaderFilesCanBeFound(UClass* TargetClass)
{
	FString TargetHeadFilePath = CodeGenerator->GetClassHeadFilePath(TargetClass->GetPrefixCPP() + TargetClass->GetName());
	return !TargetHeadFilePath.IsEmpty();
}

bool FReplicatorGeneratorManager::IsIgnoredActor(UClass* TargetClass)
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

bool FReplicatorGeneratorManager::GeneratedReplicators(TArray<UClass*> Targets)
{
	UE_LOG(LogChanneldRepGenerator, Display, TEXT("Start generating %d replicators"), Targets.Num());

	TArray<FString> IncludeActorCodes, RegisterReplicatorCodes;

	FReplicatorCodeBundle ReplicatorCodeBundle;

	CodeGenerator->Generate(Targets, ReplicatorCodeBundle);
	FString WriteCodeFileMessage;

	// Generate replicator code file
	for (FReplicatorCode& ReplicatorCode : ReplicatorCodeBundle.ReplicatorCodes)
	{
		WriteCodeFile(DefaultModuleDirPath / ReplicatorCode.HeadFileName, ReplicatorCode.HeadCode, WriteCodeFileMessage);
		WriteCodeFile(DefaultModuleDirPath / ReplicatorCode.CppFileName, ReplicatorCode.CppCode, WriteCodeFileMessage);
		WriteProtoFile(DefaultModuleDirPath / ReplicatorCode.ProtoFileName, ReplicatorCode.ProtoDefinitions, WriteCodeFileMessage);
		UE_LOG(
			LogChanneldRepGenerator,
			Verbose,
			TEXT("The replicator for the target class [%s] was generated successfully.\n    Package path: %s\n    Head file: %s\n    CPP file: %s\n    Proto file: %s\n"),
			*ReplicatorCode.Target->GetOriginActorName(),
			*ReplicatorCode.Target->GetPackagePathName(),
			*ReplicatorCode.HeadFileName,
			*ReplicatorCode.CppFileName,
			*ReplicatorCode.ProtoFileName
		);
	}
	// Generate replicator register code file
	WriteCodeFile(DefaultModuleDirPath / GenManager_RepRegisterFile, ReplicatorCodeBundle.RegisterReplicatorFileCode, WriteCodeFileMessage);

	// Generate global struct declarations file
	WriteCodeFile(DefaultModuleDirPath / GenManager_GlobalStructHeaderFile, ReplicatorCodeBundle.GlobalStructCodes, WriteCodeFileMessage);

	WriteProtoFile(DefaultModuleDirPath / GenManager_GlobalStructProtoFile, ReplicatorCodeBundle.GlobalStructProtoDefinitions, WriteCodeFileMessage);

	UE_LOG(
		LogChanneldRepGenerator,
		Display,
		TEXT("The generation of replicators is completed, %d replicators need to be generated, a total of %d replicators are generated"),
		Targets.Num(), ReplicatorCodeBundle.ReplicatorCodes.Num()
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
	bool bSuccess = WriteCodeFile(FilePath, ProtoContent, ResultMessage);
	IModuleInterface* Module = FModuleManager::Get().GetModule(GenManager_ProtobufModuleName);
	if (Module == nullptr)
	{
		ResultMessage = TEXT("\"ue4-protobuf\" plugin not found");
		return false;
	}
	// FProtobufEditorModule* ProtoModule = static_cast<FProtobufEditorModule*>(Module);
	// ProtoModule->PluginButtonClicked();

	return bSuccess;
}

FString FReplicatorGeneratorManager::GetDefaultModuleDir()
{
	TArray<FString> FoundModuleBuildFilePaths;
	IFileManager::Get().FindFilesRecursive(
		FoundModuleBuildFilePaths,
		*FPaths::GameSourceDir(),
		TEXT("*.Build.cs"),
		true, false, false
	);

	// Find all module build file under game source dir
	TMap<FString, FString> ModuleBuildFilePathMapping;
	TMap<FString, int32> ModulePathDepthMapping;
	for (FString BuildFilePath : FoundModuleBuildFilePaths)
	{
		FString ModuleBaseName = FPaths::GetCleanFilename(BuildFilePath);
		// Remove the extension
		const int32 ExtPos = ModuleBaseName.Find(TEXT("."), ESearchCase::CaseSensitive);
		if (ExtPos != INDEX_NONE)
		{
			ModuleBaseName.LeftInline(ExtPos);
		}
		ModuleBuildFilePathMapping.Add(ModuleBaseName, BuildFilePath);

		// Get parent dir count of the module
		int32 ModulePathDepth = 0;
		TCHAR* Start = const_cast<TCHAR*>(*BuildFilePath);
		for (TCHAR* Data = Start + BuildFilePath.Len(); Data != Start;)
		{
			--Data;
			if (*Data == TEXT('/') || *Data == TEXT('\\'))
			{
				++ModulePathDepth;
			}
		}
		ModulePathDepthMapping.Add(ModuleBaseName, ModulePathDepth);
	}

	// If a module is not included in .uproject->Modules, the module will be unavailable at runtime.
	// Modules will be available at runtime when they were registered to FModuleManager.
	TArray<FModuleStatus> ModuleStatuses;
	FModuleManager::Get().QueryModules(ModuleStatuses);
	FString DefaultModuleDir;
	int32 MinimalPathDepth = INT32_MAX;
	for (FModuleStatus ModuleStatus : ModuleStatuses)
	{
		if (ModuleBuildFilePathMapping.Contains(ModuleStatus.Name))
		{
			// Get the 1st minimal path depth module in FModuleManager as the default module
			const int32 Depth = ModulePathDepthMapping.FindChecked(ModuleStatus.Name);
			if (Depth < MinimalPathDepth)
			{
				DefaultModuleDir = FPaths::GetPath(ModuleBuildFilePathMapping.FindChecked(ModuleStatus.Name));
				MinimalPathDepth = Depth;
			}
		}
	}
	return DefaultModuleDir;
}

TArray<FString> FReplicatorGeneratorManager::GetGeneratedTargetClass()
{
	TArray<FString> HeadFiles;
	IFileManager::Get().FindFiles(HeadFiles, *DefaultModuleDirPath, *CodeGen_HeadFileExtension);

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
	IFileManager::Get().FindFiles(AllCodeFiles, *DefaultModuleDirPath);
	for (const FString& FilePath : AllCodeFiles)
	{
		IFileManager::Get().Delete(*(DefaultModuleDirPath / FilePath));
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

TArray<FString> FReplicatorGeneratorManager::GetAllGeneratedProtoFilePath()
{
	TArray<FString> AllCodeFiles;
	IFileManager::Get().FindFiles(AllCodeFiles, *DefaultModuleDirPath, *CodeGen_ProtoFileExtension);
	return AllCodeFiles;
}
