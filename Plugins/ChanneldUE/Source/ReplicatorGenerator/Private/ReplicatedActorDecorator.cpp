#include "ReplicatedActorDecorator.h"

#include "PropertyDecoratorFactory.h"
#include "ReplicatorTemplate/CppReplicatorTemplate.h"

FReplicatedActorDecorator::FReplicatedActorDecorator(const UClass* TargetActorClass)
{
	Target = TargetActorClass;
	bIsBlueprintGenerated = Target->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
}

void FReplicatedActorDecorator::Init()
{
	FPropertyDecoratorFactory& PropertyDecoratorFactory = FPropertyDecoratorFactory::Get();
	// Construct all property decorator
	for (TFieldIterator<FProperty> It(Target, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		FProperty* Property = *It;
		if (Property->Owner != Target)
		{
			continue;
		}
		if (Property->HasAnyPropertyFlags(CPF_Net))
		{
			TSharedPtr<FPropertyDecorator> PropertyDecoratorPtr = PropertyDecoratorFactory.GetPropertyDecorator(Property, this);
			if (PropertyDecoratorPtr.IsValid())
			{
				Properties.Emplace(PropertyDecoratorPtr);
			}
		}
	}
	for (TSharedPtr<FPropertyDecorator> PropertyDecoratorPtr : Properties)
	{
		PropertyDecoratorPtr->Init();
	}
	// Construct all rpc func decorator
	TArray<FName> FunctionNames;
	Target->GenerateFunctionList(FunctionNames);
	for (const FName FuncName : FunctionNames)
	{
		UFunction* Func = Target->FindFunctionByName(FuncName, EIncludeSuperFlag::ExcludeSuper);
		if (!Func->HasAnyFunctionFlags(FUNC_Net)) { continue; }
		
		RPCs.Add(MakeShareable(new FRPCDecorator(Func, this)));
	}
}

void FReplicatedActorDecorator::Init(const FModuleInfo& InModuleBelongTo)
{
	ModuleBelongTo = FModuleInfo(InModuleBelongTo);
	Init();
}

FString FReplicatedActorDecorator::GetActorName()
{
	return Target->GetName();
}

FString FReplicatedActorDecorator::GetActorCPPClassName()
{
	return Target->GetPrefixCPP() + Target->GetName();
}

FString FReplicatedActorDecorator::GetActorHeaderIncludePath()
{
	return ModuleBelongTo.RelativeToModule;
}

FString FReplicatedActorDecorator::GetAdditionalIncludeFiles()
{
	TSet<FString> IncludeFileSet;
	for (auto PropDecorator : Properties)
	{
		IncludeFileSet.Append(PropDecorator->GetAdditionalIncludes());
	}
	TArray<FString> IncludeFiles = IncludeFileSet.Array();
	FString Result;
	for (FString IncludeFile : IncludeFiles)
	{
		Result += FString::Printf(TEXT("#include \"%s\"\n"), *IncludeFile);
	}
	return Result;
}

FString FReplicatedActorDecorator::GetReplicatorClassName(bool WithPrefix /* = true */)
{
	FString ClassName = TEXT("Channeld") + Target->GetName() + TEXT("Replicator");
	return WithPrefix ? TEXT("F") + ClassName : ClassName;
}

FString FReplicatedActorDecorator::GetCode_IndirectlyAccessiblePropertyPtrDeclarations()
{
	FString Result;
	for (TSharedPtr<FPropertyDecorator> Property : Properties)
	{
		if (!Property->IsDirectlyAccessible())
		{
			Result += Property->GetDeclaration_PropertyPtr() + TEXT(";\n");
		}
	}
	return Result;
}

FString FReplicatedActorDecorator::GetCode_AssignPropertyPointers(const FString& TargetInstance)
{
	FString Result;
	for (TSharedPtr<FPropertyDecorator> Property : Properties)
	{
		if (!Property->IsDirectlyAccessible())
		{
			Result += FString::Printf(
				TEXT("{ %s; }\n"),
				*Property->GetCode_AssignPropPointer(
					FString::Printf(TEXT("%s.Get()"), *TargetInstance),
					Property->GetPointerName()
				)
			);
		}
	}
	return Result;
}


FString FReplicatedActorDecorator::GetProtoPackageName()
{
	return Target->GetName().ToLower() + "pb";
}

FString FReplicatedActorDecorator::GetProtoNamespace()
{
	return GetProtoPackageName();
}

FString FReplicatedActorDecorator::GetProtoStateMessageType()
{
	return Target->GetName() + TEXT("State");
}

FString FReplicatedActorDecorator::GetCode_AllPropertiesSetDeltaState(const FString& TargetInstance, const FString& FullStateName, const FString& DeltaStateName)
{
	if (Properties.Num() == 0)
	{
		return TEXT("");
	}
	FStringBuilderBase SetDeltaStateCodeBuilder;
	for (const TSharedPtr<FPropertyDecorator> Property : Properties)
	{
		SetDeltaStateCodeBuilder.Append(Property->GetCode_SetDeltaState(TargetInstance, FullStateName, DeltaStateName));
	}
	return SetDeltaStateCodeBuilder.ToString();
}

FString FReplicatedActorDecorator::GetCode_AllPropertiesOnStateChange(const FString& TargetInstance, const FString& NewStateName)
{
	if (Properties.Num() == 0)
	{
		return TEXT("");
	}
	FStringBuilderBase OnChangeStateCodeBuilder;
	for (const TSharedPtr<FPropertyDecorator> Property : Properties)
	{
		OnChangeStateCodeBuilder.Append(Property->GetCode_OnStateChange(TargetInstance, NewStateName));
	}
	return OnChangeStateCodeBuilder.ToString();
}

FString FReplicatedActorDecorator::GetDefinition_ProtoStateMessage()
{
	FString FieldDefinitions;
	for (int32 i = 0; i < Properties.Num(); i++)
	{
		const TSharedPtr<FPropertyDecorator> Property = Properties[i];
		FieldDefinitions += Property->GetDefinition_ProtoField(i + 1) + TEXT(";\n");
	}
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_StateMessageType"), GetProtoStateMessageType());
	FormatArgs.Add(TEXT("Declare_ProtoFields"), FieldDefinitions);

	return FString::Format(CodeGen_ProtoStateMessageTemplate, FormatArgs);
}

bool FReplicatedActorDecorator::IsBlueprintType()
{
	return bIsBlueprintGenerated;
}

int32 FReplicatedActorDecorator::GetRPCNum()
{
	return RPCs.Num();
}
