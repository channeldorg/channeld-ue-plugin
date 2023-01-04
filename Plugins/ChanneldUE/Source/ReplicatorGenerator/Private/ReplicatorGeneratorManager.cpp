#include "ReplicatorGeneratorManager.h"

#include "ReplicatorGeneratorDefinition.h"
#include "ProtobufEditor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "Engine/SCS_Node.h"
#include "GameFramework/Character.h"
#include "HAL/FileManagerGeneric.h"
#include "Replication/ChanneldReplicationComponent.h"

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

bool FReplicatorGeneratorManager::HasReplicatedPropertyOrRPC(UClass* TargetClass)
{
	for (TFieldIterator<FProperty> It(TargetClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		if ((*It)->HasAnyPropertyFlags(CPF_Net))
		{
			return true;
		}
	}
	TArray<FName> FunctionNames;
	TargetClass->GenerateFunctionList(FunctionNames);
	for (const FName FuncName : FunctionNames)
	{
		const UFunction* Func = TargetClass->FindFunctionByName(FuncName, EIncludeSuperFlag::IncludeSuper);
		if (Func->HasAnyFunctionFlags(FUNC_Net))
		{
			return true;
		}
	}
	return false;
}

bool FReplicatorGeneratorManager::HasRepComponent(UClass* TargetClass)
{
	for (TFieldIterator<FProperty> It(TargetClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		FProperty* Property = *It;

		if (Property->IsA<FObjectProperty>())
		{
			FObjectProperty* ObjProperty = CastFieldChecked<FObjectProperty>(Property);
			if (ObjProperty->PropertyClass->IsChildOf(UChanneldReplicationComponent::StaticClass()))
			{
				return true;
			}
		}
	}
	return false;
}

TArray<UClass*> FReplicatorGeneratorManager::GetActorsWithReplicationComp(const TArray<UClass*>& IgnoreActors)
{
	FFileManagerGeneric FileManager;

	TSet<UClass*> IgnoreActorsSet;
	for (UClass* Class : IgnoreActors)
	{
		IgnoreActorsSet.Add(Class);
	}

	TArray<UClass*> ModifiedClasses;

	// This array contains loaded UBlueprintGeneratedClass and Cpp's UClass,
	// but FindComponentByClass cannot find components added from component panel or ConstructionScript,
	// ignore UBlueprintGeneratedClass temporarily and process Cpp's UClass first.
	TArray<UObject*> AllUClasses;
	GetObjectsOfClass(UClass::StaticClass(), AllUClasses);
	// AllUClasses.Add(ATRG_F005_CPP_Parent::StaticClass());
	for (UObject* ClassObj : AllUClasses)
	{
		UClass* Class = CastChecked<UClass>(ClassObj);
		if (IgnoreActorsSet.Contains(Class))
		{
			UE_LOG(LogChanneldRepGenerator, Log, TEXT("Ignore generating replicator for %s"), *Class->GetName());
			continue;
		}
		if (!Class->IsChildOf(AActor::StaticClass()))
		{
			continue;
		}
		if (Class->GetName().StartsWith(TEXT("SKEL_")) || Class->GetName().StartsWith(TEXT("REINST_")))
		{
			continue;
		}
		if (!HasRepComponent(Class))
		{
			continue;
		}
		FString TargetHeadFilePath = CodeGenerator->GetClassHeadFilePath(Class->GetPrefixCPP() + Class->GetName());
		if (TargetHeadFilePath.IsEmpty())
		{
			continue;
		}
		ModifiedClasses.Add(Class);
	}

	// Load all blueprint assets
	TArray<FAssetData> ArrayAssetData;
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FARFilter filter;
	filter.ClassNames.Add(UBlueprint::StaticClass()->GetFName());
	filter.bRecursiveClasses = true;
	AssetRegistryModule.Get().GetAssets(filter, ArrayAssetData);

	TArray<UBlueprint*> AllBlueprints;
	for (FAssetData AssetData : ArrayAssetData)
	{
		FString PackageFileName;
		FString PackageFile;
		if (FPackageName::TryConvertLongPackageNameToFilename(AssetData.PackageName.ToString(), PackageFileName) &&
			FPackageName::FindPackageFileWithoutExtension(PackageFileName, PackageFile))
		{
			PackageFile = FPaths::ConvertRelativePathToFull(MoveTemp(PackageFile));
		}

		UBlueprint* Asset = LoadObject<UBlueprint>(nullptr, *AssetData.ObjectPath.ToString());

		if (Asset != nullptr)
		{
			AllBlueprints.Add(Asset);
		}
	}

	for (UBlueprint* Blueprint : AllBlueprints)
	{
		UClass* GeneratedClass = Blueprint->GeneratedClass;
		if (!GeneratedClass->IsChildOf(AActor::StaticClass()))
		{
			continue;
		}
		if (IgnoreActorsSet.Contains(GeneratedClass))
		{
			UE_LOG(LogChanneldRepGenerator, Log, TEXT("Ignore generating replicator for %s"), *GeneratedClass->GetName());
			continue;
		}
		bool bOwnReplicationComponent = false;
		TArray<UActorComponent*> CompTemplates = Blueprint->ComponentTemplates;
		if (CompTemplates.Num() > 0)
		{
			for (const UActorComponent* CompTemplate : CompTemplates)
			{
				if (CompTemplate->GetClass()->IsChildOf(UChanneldReplicationComponent::StaticClass()))
				{
					bOwnReplicationComponent = true;
					break;
				}
			}
		}

		if (!bOwnReplicationComponent)
		{
			// Find UChanneldReplicationComponent added from component panel
			const USimpleConstructionScript* CtorScript = Blueprint->SimpleConstructionScript;
			if (CtorScript == nullptr) { continue; }
			TArray<USCS_Node*> Nodes = CtorScript->GetAllNodes();
			for (const USCS_Node* Node : Nodes)
			{
				if (Node->ComponentClass->IsChildOf(UChanneldReplicationComponent::StaticClass()))
				{
					bOwnReplicationComponent = true;
					break;;
				}
			}
		}

		if (bOwnReplicationComponent)
		{
			ModifiedClasses.Add(GeneratedClass);
		}
	}

	TArray<UClass*> ParentClasses;
	TSet<UClass*> ParentClassesUnique;
	for (UClass* ActorClass : ModifiedClasses)
	{
		ParentClassesUnique.Add(ActorClass);
		UClass* ParentClass = ActorClass->GetSuperClass();
		if (ParentClass != nullptr && !ParentClassesUnique.Contains(ParentClass))
		{
			ParentClasses.Add(ParentClass);
			ParentClassesUnique.Add(ParentClass);
		}
	}

	for (int32 i = 0; i < ParentClasses.Num(); i++)
	{
		UClass* ThisClass = ParentClasses[i];
		if (!ThisClass->IsChildOf(AActor::StaticClass()))
		{
			continue;
		}
		if (IgnoreActorsSet.Contains(ThisClass))
		{
			UE_LOG(LogChanneldRepGenerator, Log, TEXT("Ignore generating replicator for %s"), *ThisClass->GetName());
			continue;
		}
		ModifiedClasses.Add(ThisClass);
		UClass* ParentClass = ThisClass->GetSuperClass();
		if (ParentClass != nullptr && !ParentClassesUnique.Contains(ParentClass))
		{
			ParentClasses.Add(ParentClass);
			ParentClassesUnique.Add(ParentClass);
		}
	}

	return ModifiedClasses;
}

bool FReplicatorGeneratorManager::GenerateAllReplicators()
{
	RemoveGeneratedCode();
	CodeGenerator->RefreshModuleInfoByClassName();
	// bool Success = false;
	// const FPrevCodeGeneratedInfo PrevCodeGeneratedInfo = LoadPrevCodeGeneratedInfo(GenManager_PrevCodeGeneratedInfoPath, Success);
	TArray<UClass*> TargetActorClasses = GetActorsWithReplicationComp(IgnoreActorClasses);

	TSet<FString> GeneratedClassNameSet(GetGeneratedTargetClass());
	TSet<FString> TargetClassNameSet;
	TargetClassNameSet.Reserve(TargetActorClasses.Num());
	for (const UClass* ActorClass : TargetActorClasses)
	{
		TargetClassNameSet.Add(ActorClass->GetName());
	}
	RemoveGeneratedReplicators(GeneratedClassNameSet.Difference(TargetClassNameSet).Array());

	TArray<FString> IncludeActorCodes, RegisterReplicatorCodes;

	FReplicatorCodeBundle ReplicatorCodeBundle;

	CodeGenerator->Generate(TargetActorClasses, ReplicatorCodeBundle);
	FString WriteCodeFileMessage;

	// Generate replicator code file
	for (FReplicatorCode& ReplicatorCode : ReplicatorCodeBundle.ReplicatorCodes)
	{
		WriteCodeFile(DefaultModuleDirPath / ReplicatorCode.HeadFileName, ReplicatorCode.HeadCode, WriteCodeFileMessage);
		WriteCodeFile(DefaultModuleDirPath / ReplicatorCode.CppFileName, ReplicatorCode.CppCode, WriteCodeFileMessage);
		WriteProtoFile(DefaultModuleDirPath / ReplicatorCode.ProtoFileName, ReplicatorCode.ProtoDefinitions, WriteCodeFileMessage);
	}
	// Generate replicator register code file
	WriteCodeFile(DefaultModuleDirPath / GenManager_RepRegisterFile, ReplicatorCodeBundle.RegisterReplicatorFileCode, WriteCodeFileMessage);

	// Generate global struct declarations file
	WriteCodeFile(DefaultModuleDirPath / GenManager_GlobalStructHeaderFile, ReplicatorCodeBundle.GlobalStructCodes, WriteCodeFileMessage);
	WriteProtoFile(DefaultModuleDirPath / GenManager_GlobalStructProtoFile, ReplicatorCodeBundle.GlobalStructProtoDefinitions, WriteCodeFileMessage);

	// FPrevCodeGeneratedInfo ThisCodeGeneratedInfo;
	// ThisCodeGeneratedInfo.GeneratedTime = FDateTime::Now();
	// SavePrevCodeGeneratedInfo(ThisCodeGeneratedInfo, GenManager_PrevCodeGeneratedInfoPath, Success);
	return true;
}

bool FReplicatorGeneratorManager::GeneratedReplicators(TArray<UClass*> Targets)
{
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
	}
	// Generate replicator register code file
	WriteCodeFile(DefaultModuleDirPath / GenManager_RepRegisterFile, ReplicatorCodeBundle.RegisterReplicatorFileCode, WriteCodeFileMessage);

	// Generate global struct declarations file
	WriteCodeFile(DefaultModuleDirPath / GenManager_GlobalStructHeaderFile, ReplicatorCodeBundle.GlobalStructCodes, WriteCodeFileMessage);

	WriteProtoFile(DefaultModuleDirPath / GenManager_GlobalStructProtoFile, ReplicatorCodeBundle.GlobalStructProtoDefinitions, WriteCodeFileMessage);

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
