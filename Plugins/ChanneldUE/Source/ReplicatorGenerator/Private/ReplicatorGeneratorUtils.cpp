#include "ReplicatorGeneratorUtils.h"

#include "Engine/SCS_Node.h"
#include "Internationalization/Regex.h"
#include "Replication/ChanneldReplicationComponent.h"

namespace ChanneldReplicatorGeneratorUtils
{
	void FObjectLoadedListener::StartListen()
	{
		GUObjectArray.AddUObjectCreateListener(this);
	}

	void FObjectLoadedListener::StopListen()
	{
		GUObjectArray.RemoveUObjectCreateListener(this);
	}

	void FObjectLoadedListener::NotifyUObjectCreated(const UObjectBase* Object, int32 Index)
	{
		UClass* LoadedClass = Object->GetClass();
		if (NeedToGenerateReplicator(LoadedClass))
		{
			LoadedRepClasses.Add(LoadedClass);
		}
	}

	void FObjectLoadedListener::OnUObjectArrayShutdown()
	{
		GUObjectArray.RemoveUObjectCreateListener(this);
	}

	bool HasReplicatedProperty(UClass* TargetClass)
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

	bool HasRPC(UClass* TargetClass)
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

	bool HasReplicatedPropertyOrRPC(UClass* TargetClass)
	{
		return HasReplicatedProperty(TargetClass) || HasRPC(TargetClass);
	}

	bool HasRepComponent(UClass* TargetClass)
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
		if (UBlueprintGeneratedClass* TargetBPClass = Cast<UBlueprintGeneratedClass>(TargetClass))
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

	bool NeedToGenerateReplicator(UClass* TargetClass)
	{
		const FString ClassName = TargetClass->GetName();
		return
			!FReplicatorGeneratorManager::Get().IsIgnoredActor(TargetClass) &&
			TargetClass->IsChildOf(AActor::StaticClass()) &&
			!(ClassName.StartsWith(TEXT("SKEL_")) || ClassName.StartsWith(TEXT("REINST_"))) &&
			// HasRepComponent(TargetClass) &&
			HasReplicatedPropertyOrRPC(TargetClass);
	}

	bool IsCompilableClassName(const FString& ClassName)
	{
		FRegexPattern MatherPatter(TEXT("[^a-zA-Z0-9_]"));
		FRegexMatcher Matcher(MatherPatter, ClassName);
		if (Matcher.FindNext())
		{
			return false;
		}
		return true;
	}

	FString GetUECmdBinary()
	{
		FString Binary;
#if ENGINE_MAJOR_VERSION > 4
		Binary = TEXT("UnrealEditor");
#else
		Binary = TEXT("UE4Editor");
#endif

		FString ConfigutationName = ANSI_TO_TCHAR(COMPILER_CONFIGURATION_NAME);
		bool bIsDevelopment = ConfigutationName.Equals(TEXT("Development"));

#if PLATFORM_WINDOWS
		FString PlatformName;
#if PLATFORM_64BITS
		PlatformName = TEXT("Win64");
#else
		PlatformName = TEXT("Win32");
#endif

		return FPaths::Combine(
			FPaths::ConvertRelativePathToFull(FPaths::EngineDir()),
			TEXT("Binaries"), PlatformName, FString::Printf(TEXT("%s%s-Cmd.exe"), *Binary, bIsDevelopment ? TEXT("") : *FString::Printf(TEXT("-%s-%s"), *PlatformName, *ConfigutationName)));
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

}
