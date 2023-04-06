#include "ReplicatorGeneratorUtils.h"

#include "Engine/LevelScriptActor.h"
#include "Engine/SCS_Node.h"
#include "Internationalization/Regex.h"
#include "Replication/ChanneldReplicationComponent.h"

namespace ChanneldReplicatorGeneratorUtils
{
	void FReplicationActorFilter::StartListen()
	{
		GUObjectArray.AddUObjectCreateListener(this);
	}

	void FReplicationActorFilter::StopListen()
	{
		GUObjectArray.RemoveUObjectCreateListener(this);
	}

	void FReplicationActorFilter::NotifyUObjectCreated(const UObjectBase* Object, int32 Index)
	{
		const UClass* LoadedClass = Object->GetClass();
		bool Condition = false;
		switch (FilterRule)
		{
		case EFilterRule::HasRepComponent:
			Condition = HasRepComponent(LoadedClass);
			break;
		case EFilterRule::NeedToGenerateReplicator:
			Condition = TargetToGenerateReplicator(LoadedClass);
			break;
		case EFilterRule::Replication:
			Condition = TargetToGenerateChannelDataField(LoadedClass);
			break;
		}
		AllLoadedClasses.Add(LoadedClass);
		if (Condition)
		{
			FilteredClasses.Add(LoadedClass);
		}
	}

	void FReplicationActorFilter::OnUObjectArrayShutdown()
	{
		GUObjectArray.RemoveUObjectCreateListener(this);
	}

	TArray<const UClass*> GetChanneldUEBuiltinClasses()
	{
		return ChanneldUEBuiltinClasses;
	}

	bool IsChanneldUEBuiltinClass(const UClass* TargetClass)
	{
		return ChanneldUEBuiltinClassSet.Contains(TargetClass);
	}

	bool IsChanneldUEBuiltinSingletonClass(const UClass* TargetClass)
	{
		return TargetClass == AGameStateBase::StaticClass();
	}

	bool HasReplicatedProperty(const UClass* TargetClass)
	{
		for (TFieldIterator<FProperty> It(TargetClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			if ((*It)->HasAnyPropertyFlags(CPF_Net))
			{
				return true;
			}
		}
		return false;
	}

	bool HasRPC(const UClass* TargetClass)
	{
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

	bool HasReplicatedPropertyOrRPC(const UClass* TargetClass)
	{
		return HasReplicatedProperty(TargetClass) || HasRPC(TargetClass);
	}

	bool HasTimelineComponent(const UClass* TargetClass)
	{
		const UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(TargetClass);
		return BPClass != nullptr && BPClass->Timelines.Num() > 0;
	}

	bool HasRepComponent(const UClass* TargetClass)
	{
		if (!TargetClass->IsChildOf(AActor::StaticClass()))
		{
			return false;
		}
		for (TFieldIterator<FProperty> It(TargetClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			FProperty* Property = *It;

			if (Property->IsA<FObjectProperty>())
			{
				const FObjectProperty* ObjProperty = CastFieldChecked<FObjectProperty>(Property);
				if (ObjProperty->PropertyClass->IsChildOf(UChanneldReplicationComponent::StaticClass()))
				{
					return true;
				}
			}
		}
		if (const UBlueprintGeneratedClass* TargetBPClass = Cast<UBlueprintGeneratedClass>(TargetClass))
		{
			TArray<UActorComponent*> CompTemplates = TargetBPClass->ComponentTemplates;
			if (CompTemplates.Num() > 0)
			{
				for (const UActorComponent* CompTemplate : CompTemplates)
				{
					if (CompTemplate->GetClass()->IsChildOf(UChanneldReplicationComponent::StaticClass()))
					{
						return true;
					}
				}
			}

			// Find UChanneldReplicationComponent added from component panel
			const USimpleConstructionScript* CtorScript = TargetBPClass->SimpleConstructionScript;
			if (CtorScript == nullptr)
			{
				return false;
			}
			TArray<USCS_Node*> Nodes = CtorScript->GetAllNodes();
			for (const USCS_Node* Node : Nodes)
			{
				if (Node->ComponentClass->IsChildOf(UChanneldReplicationComponent::StaticClass()))
				{
					return true;
				}
			}
		}
		return false;
	}

	TArray<const UClass*> GetComponentClasses(const UClass* TargetClass)
	{
		TSet<const UClass*> ComponentClasses;
		for (TFieldIterator<FProperty> It(TargetClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			FProperty* Property = *It;

			if (Property->IsA<FObjectProperty>())
			{
				const FObjectProperty* ObjProperty = CastFieldChecked<FObjectProperty>(Property);
				if (ObjProperty->PropertyClass->IsChildOf(UActorComponent::StaticClass()))
				{
					ComponentClasses.Add(ObjProperty->PropertyClass);
				}
			}
		}
		if (const UBlueprintGeneratedClass* TargetBPClass = Cast<UBlueprintGeneratedClass>(TargetClass))
		{
			TArray<UActorComponent*> CompTemplates = TargetBPClass->ComponentTemplates;
			if (CompTemplates.Num() > 0)
			{
				for (const UActorComponent* CompTemplate : CompTemplates)
				{
					if (CompTemplate->GetClass()->IsChildOf(UActorComponent::StaticClass()))
					{
						ComponentClasses.Add(CompTemplate->GetClass());
					}
				}
			}

			// Find UChanneldReplicationComponent added from component panel
			const USimpleConstructionScript* CtorScript = TargetBPClass->SimpleConstructionScript;
			if (CtorScript != nullptr)
			{
				TArray<USCS_Node*> Nodes = CtorScript->GetAllNodes();
				for (const USCS_Node* Node : Nodes)
				{
					if (Node->ComponentClass->IsChildOf(UActorComponent::StaticClass()))
					{
						ComponentClasses.Add(Node->ComponentClass);
					}
				}
			}
		}
		return ComponentClasses.Array();
	}

	bool TargetToGenerateReplicator(const UClass* TargetClass)
	{
		return !IsChanneldUEBuiltinClass(TargetClass) && TargetToGenerateChannelDataField(TargetClass);
	}

	bool TargetToGenerateChannelDataField(const UClass* TargetClass)
	{
		const FString ClassName = TargetClass->GetName();
		return
			(TargetClass->IsChildOf(AActor::StaticClass()) || TargetClass->IsChildOf(UActorComponent::StaticClass())) &&
			!TargetClass->IsChildOf(ALevelScriptActor::StaticClass()) &&
			!(ClassName.StartsWith(TEXT("SKEL_")) || ClassName.StartsWith(TEXT("REINST_"))) &&
			HasReplicatedPropertyOrRPC(TargetClass);
	}

	bool ContainsUncompilableChar(const FString& Test)
	{
		const FRegexPattern MatherPatter(TEXT("[^a-zA-Z0-9_]"));
		FRegexMatcher Matcher(MatherPatter, Test);
		if (Matcher.FindNext())
		{
			return true;
		}
		return false;
	}

	bool IsCompilableClassName(const FString& ClassName)
	{
		if (!iswalpha(ClassName[0]))
		{
			return false;
		}
		return !ContainsUncompilableChar(ClassName);
	}

	FString ReplaceUncompilableChar(const FString& String, const FString& ReplaceTo)
	{
		const FRegexPattern MatherPatter(TEXT("[^a-zA-Z0-9_]"));
		FRegexMatcher Matcher(MatherPatter, String);
		FString Result = String;
		while (Matcher.FindNext())
		{
			Result.ReplaceInline(*Matcher.GetCaptureGroup(0), *ReplaceTo);
		}
		return Result;
	}

	FString GetDefaultModuleDir()
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

	FString GetUECmdBinary()
	{
		FString Binary;
#if ENGINE_MAJOR_VERSION > 4
		Binary = TEXT("UnrealEditor");
#else
		Binary = TEXT("UE4Editor");
#endif

		const FString ConfigurationName = ANSI_TO_TCHAR(COMPILER_CONFIGURATION_NAME);
		bool bIsDevelopment = ConfigurationName.Equals(TEXT("Development"));

#if PLATFORM_WINDOWS
		FString PlatformName;
#if PLATFORM_64BITS
		PlatformName = TEXT("Win64");
#else
		PlatformName = TEXT("Win32");
#endif

		return FPaths::Combine(
			FPaths::ConvertRelativePathToFull(FPaths::EngineDir()),
			TEXT("Binaries"), PlatformName, FString::Printf(TEXT("%s%s-Cmd.exe"), *Binary, bIsDevelopment ? TEXT("") : *FString::Printf(TEXT("-%s-%s"), *PlatformName, *ConfigurationName)));
#endif

#if PLATFORM_MAC
		return FPaths::Combine(
				FPaths::ConvertRelativePathToFull(FPaths::EngineDir()),
				TEXT("Binaries"),TEXT("Mac"),
				FString::Printf(TEXT("%s%s-Cmd"),*Binary,
					bIsDevelopment ? TEXT("") : *FString::Printf(TEXT("-Mac-%s"),*ConfigutationName)));
#endif
		return TEXT("");
	}

	FString GetHashString(const FString& Target)
	{
		const int32 Hash = GetTypeHash(Target);
		if (Hash < 0)
		{
			return FString::Printf(TEXT("_%d"), -Hash);
		}
		else
		{
			return FString::Printf(TEXT("%d"), Hash);
		}
	}

	void EnsureRepGenIntermediateDir()
	{
		IFileManager& FileManager = IFileManager::Get();
		if (!FileManager.DirectoryExists(*GenManager_IntermediateDir))
		{
			FileManager.MakeDirectory(*GenManager_IntermediateDir, true);
		}
	}
}
