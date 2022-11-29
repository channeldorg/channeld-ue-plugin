#include "ReplicatorGeneratorManager.h"

#include "ProtobufEditor.h"

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

bool FReplicatorGeneratorManager::GenerateAllReplicator()
{
}

bool FReplicatorGeneratorManager::GenerateReplicator(UClass* Target)
{
	FString ResultMessage;
	FReplicatorCodeGroup ReplicatorCode;
	CodeGenerator->RefreshModuleInfoByClassName();
	const bool GetCodeSuccess = CodeGenerator->GenerateCode(Target, ReplicatorCode, ResultMessage);
	if (!GetCodeSuccess)
	{
		return false;
	}
	FString WriteCodeFileMessage;
	const FString DefaultGameModuleDir = GetDefaultModuleDir();
	const FString ReplicatorClassName = ReplicatorCode.Target->GetReplicatorClassName(false);
	WriteCodeFile(DefaultGameModuleDir / GenManager_GeneratedCodeDir / ReplicatorCode.HeadFileName, ReplicatorCode.HeadCode, WriteCodeFileMessage);
	WriteCodeFile(DefaultGameModuleDir / GenManager_GeneratedCodeDir / ReplicatorCode.CppFileName, ReplicatorCode.CppCode, WriteCodeFileMessage);
	WriteProtoFile(DefaultGameModuleDir / GenManager_GeneratedCodeDir / ReplicatorCode.ProtoFileName, ReplicatorCode.ProtoDefinitions, WriteCodeFileMessage);
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
	FProtobufEditorModule* ProtoModule = static_cast<FProtobufEditorModule*>(Module);
	ProtoModule->PluginButtonClicked();

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
