#include "ReplicatorGeneratorManager.h"

#include "ReplicatorGeneratorDefinition.h"
#include "ProtobufEditor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "Engine/SCS_Node.h"
#include "GameFramework/Character.h"
#include "Replication/ChanneldReplicationComponent.h"

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

bool FReplicatorGeneratorManager::HasReplicatedPropertyOrRPC(UClass* TargetClass)
{
	for (TFieldIterator<FProperty> It(TargetClass); It; ++It)
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

TArray<UClass*> FReplicatorGeneratorManager::GetActorsWithReplicationComp(TArray<UClass*> IgnoreActors)
{
	TSet<UClass*> IgnoreActorsSet;
	for (UClass* Class : IgnoreActors)
	{
		IgnoreActorsSet.Add(Class);
	}

	TArray<UClass*> ActorsWithReplicationComp;

	// This array contains loaded UBlueprintGeneratedClass and Cpp's UClass,
	// but FindComponentByClass cannot find components added from component panel or ConstructionScript,
	// ignore UBlueprintGeneratedClass temporarily and process Cpp's UClass first.
	TArray<UObject*> AllUClasses;
	GetObjectsOfClass(UClass::StaticClass(), AllUClasses);
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
		AActor* DefaultActor = CastChecked<AActor>(Class->GetDefaultObject());
		const UChanneldReplicationComponent* Comp = DefaultActor->FindComponentByClass<UChanneldReplicationComponent>();
		if (Comp == nullptr)
		{
			continue;
		}
		ActorsWithReplicationComp.Add(Class);
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
			ActorsWithReplicationComp.Add(GeneratedClass);
		}
	}

	TArray<UClass*> ParentClasses;
	TSet<UClass*> ParentClassesUnique;
	for (UClass* ActorClass : ActorsWithReplicationComp)
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
		ActorsWithReplicationComp.Add(ThisClass);
		UClass* ParentClass = ThisClass->GetSuperClass();
		if (ParentClass != nullptr && !ParentClassesUnique.Contains(ParentClass))
		{
			ParentClasses.Add(ParentClass);
			ParentClassesUnique.Add(ParentClass);
		}
	}

	return ActorsWithReplicationComp;
}

bool FReplicatorGeneratorManager::GeneratedAllReplicators()
{
	TArray<UClass*> TargetActorClasses = GetActorsWithReplicationComp(IgnoreActorClasses);
	const FString GeneratedDir = GetDefaultModuleDir() / GenManager_GeneratedCodeDir;

	TArray<FString> IncludeActorCodes, RegisterReplicatorCodes;

	FReplicatorCodeBundle ReplicatorCodeBundle;

	CodeGenerator->Generate(TargetActorClasses, ReplicatorCodeBundle);
	FString WriteCodeFileMessage;

	// Generate replicator code file
	for (FReplicatorCode& ReplicatorCode : ReplicatorCodeBundle.ReplicatorCodes)
	{
		// TODO Transaction
		WriteCodeFile(GeneratedDir / ReplicatorCode.HeadFileName, ReplicatorCode.HeadCode, WriteCodeFileMessage);
		WriteCodeFile(GeneratedDir / ReplicatorCode.CppFileName, ReplicatorCode.CppCode, WriteCodeFileMessage);
		WriteProtoFile(GeneratedDir / ReplicatorCode.ProtoFileName, ReplicatorCode.ProtoDefinitions, WriteCodeFileMessage);
	}
	// Generate replicator register code file
	WriteCodeFile(GeneratedDir / GenManager_RepRegisterFile, ReplicatorCodeBundle.RegisterReplicatorFileCode, WriteCodeFileMessage);

	// Generate global struct declarations file
	WriteCodeFile(GeneratedDir / GenManager_GlobalStructHeaderFile, ReplicatorCodeBundle.GlobalStructCodes, WriteCodeFileMessage);

	return true;
}

bool FReplicatorGeneratorManager::GeneratedReplicators(UClass* Target)
{
	const FString GeneratedDir = GetDefaultModuleDir() / GenManager_GeneratedCodeDir;

	TArray<FString> IncludeActorCodes, RegisterReplicatorCodes;

	FReplicatorCodeBundle ReplicatorCodeBundle;

	CodeGenerator->Generate(TArray<UClass*>{Target}, ReplicatorCodeBundle);
	FString WriteCodeFileMessage;

	// Generate replicator code file
	for (FReplicatorCode& ReplicatorCode : ReplicatorCodeBundle.ReplicatorCodes)
	{
		// TODO Transaction
		WriteCodeFile(GeneratedDir / ReplicatorCode.HeadFileName, ReplicatorCode.HeadCode, WriteCodeFileMessage);
		WriteCodeFile(GeneratedDir / ReplicatorCode.CppFileName, ReplicatorCode.CppCode, WriteCodeFileMessage);
		WriteProtoFile(GeneratedDir / ReplicatorCode.ProtoFileName, ReplicatorCode.ProtoDefinitions, WriteCodeFileMessage);
	}
	// Generate replicator register code file
	WriteCodeFile(GeneratedDir / GenManager_RepRegisterFile, ReplicatorCodeBundle.RegisterReplicatorFileCode, WriteCodeFileMessage);

	// Generate global struct declarations file
	WriteCodeFile(GeneratedDir / GenManager_GlobalStructHeaderFile, ReplicatorCodeBundle.GlobalStructCodes, WriteCodeFileMessage);

	WriteProtoFile(GeneratedDir / GenManager_GlobalStructProtoFile, ReplicatorCodeBundle.GlobalStructProtoDefinitions, WriteCodeFileMessage);

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
