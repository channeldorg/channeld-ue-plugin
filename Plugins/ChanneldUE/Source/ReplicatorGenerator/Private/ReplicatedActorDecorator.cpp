#include "ReplicatedActorDecorator.h"

#include "PropertyDecoratorFactory.h"
#include "ReplicatorGeneratorUtils.h"
#include "GameFramework/GameStateBase.h"
#include "ReplicatorTemplate/CppReplicatorTemplate.h"

FReplicatedActorDecorator::FReplicatedActorDecorator(const UClass* TargetActorClass, const TFunction<void(FString&, bool)>& SetCompilableName)
{
	Target = TargetActorClass;
	bIsBlueprintGenerated = Target->HasAnyClassFlags(CLASS_CompiledFromBlueprint);

	TargetActorCompilableName = Target->GetName();
	if (SetCompilableName != nullptr)
	{
		SetCompilableName(
			TargetActorCompilableName,
			bIsBlueprintGenerated ? ChanneldReplicatorGeneratorUtils::IsCompilableClassName(TargetActorCompilableName) : true
		);
	}
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
	int32 IllegalPropNameIndex = 0;
	for (TSharedPtr<FPropertyDecorator> PropertyDecoratorPtr : Properties)
	{
		PropertyDecoratorPtr->Init(
			[&IllegalPropNameIndex]()
			{
				return FString::Printf(TEXT("_IllegalNameProp_%d_"), ++IllegalPropNameIndex);
			}
		);
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
	int32 IllegalParamNameIndex = 0;
	for (TSharedPtr<FPropertyDecorator> RPCDecorator : RPCs)
	{
		RPCDecorator->Init(
			[&IllegalParamNameIndex]()
			{
				return FString::Printf(TEXT("_IllegalNameParam_%d_"), ++IllegalParamNameIndex);
			}
		);
	}
}

void FReplicatedActorDecorator::Init(const FModuleInfo& InModuleBelongTo)
{
	ModuleBelongTo = FModuleInfo(InModuleBelongTo);
	Init();
}

FString FReplicatedActorDecorator::GetActorName()
{
	return TargetActorCompilableName;
}

FString FReplicatedActorDecorator::GetOriginActorName()
{
	return Target->GetName();
}

FString FReplicatedActorDecorator::GetPackagePathName()
{
	return Target->GetPackage()->GetPathName();
}

FString FReplicatedActorDecorator::GetActorCPPClassName()
{
	return Target->GetPrefixCPP() + GetActorName();
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
	for (auto RPCDecorator : RPCs)
	{
		IncludeFileSet.Append(RPCDecorator->GetAdditionalIncludes());
	}
	TArray<FString> IncludeFiles = IncludeFileSet.Array();
	if (Target->IsChildOf(UActorComponent::StaticClass()))
	{
		IncludeFiles.Add(TEXT("Engine/PackageMapClient.h"));
	}
	FString Result;
	for (FString IncludeFile : IncludeFiles)
	{
		Result.Append(FString::Printf(TEXT("#include \"%s\"\n"), *IncludeFile));
	}
	return Result;
}

FString FReplicatedActorDecorator::GetReplicatorClassName(bool WithPrefix /* = true */)
{
	FString ClassName = TEXT("Channeld") + GetActorName() + TEXT("Replicator");
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

FString FReplicatedActorDecorator::GetCode_AssignPropertyPointers()
{
	FString Result;
	for (TSharedPtr<FPropertyDecorator> Property : Properties)
	{
		if (!Property->IsDirectlyAccessible())
		{
			Result += FString::Printf(
				TEXT("{ %s; }\n"),
				*Property->GetCode_AssignPropPointer(
					FString::Printf(TEXT("%s.Get()"), *InstanceRefName),
					Property->GetPointerName()
				)
			);
		}
	}
	return Result;
}


FString FReplicatedActorDecorator::GetProtoPackageName()
{
	return GetActorName().ToLower() + "pb";
}

FString FReplicatedActorDecorator::GetProtoNamespace()
{
	return GetProtoPackageName();
}

FString FReplicatedActorDecorator::GetProtoStateMessageType()
{
	return GetActorName() + TEXT("State");
}

FString FReplicatedActorDecorator::GetCode_AllPropertiesSetDeltaState(const FString& FullStateName, const FString& DeltaStateName)
{
	if (Properties.Num() == 0)
	{
		return TEXT("");
	}
	FString SetDeltaStateCodeBuilder;
	for (const TSharedPtr<FPropertyDecorator> Property : Properties)
	{
		SetDeltaStateCodeBuilder.Append(Property->GetCode_SetDeltaState(InstanceRefName, FullStateName, DeltaStateName));
	}
	return SetDeltaStateCodeBuilder;
}

FString FReplicatedActorDecorator::GetCode_AllPropertiesOnStateChange(const FString& NewStateName)
{
	if (Properties.Num() == 0)
	{
		return TEXT("");
	}
	FString OnChangeStateCodeBuilder;
	for (const TSharedPtr<FPropertyDecorator> Property : Properties)
	{
		OnChangeStateCodeBuilder.Append(Property->GetCode_OnStateChange(InstanceRefName, NewStateName, true));
	}
	return OnChangeStateCodeBuilder;
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

FString FReplicatedActorDecorator::GetDefinition_RPCParamsMessage()
{
	FString RPCParamMessages;
	for (int32 i = 0; i < RPCs.Num(); i++)
	{
		const TSharedPtr<FRPCDecorator> RPC = RPCs[i];
		FStringFormatNamedArguments FormatArgs;
		FormatArgs.Add(TEXT("Declare_StateMessageType"), RPC->GetProtoStateMessageType());
		FormatArgs.Add(TEXT("Declare_ProtoFields"), RPC->GetDeclaration_ProtoFields());

		RPCParamMessages.Append(FString::Format(CodeGen_ProtoStateMessageTemplate, FormatArgs));
	}
	return RPCParamMessages;
}

bool FReplicatedActorDecorator::IsBlueprintType()
{
	return bIsBlueprintGenerated;
}

int32 FReplicatedActorDecorator::GetRPCNum()
{
	return RPCs.Num();
}

FString FReplicatedActorDecorator::GetCode_SerializeFunctionParams()
{
	FString SerializeParamCodes;
	bool bIsFirst = true;
	for (int32 i = 0; i < RPCs.Num(); i++)
	{
		const TSharedPtr<FRPCDecorator> RPC = RPCs[i];
		if (!bIsFirst)
		{
			SerializeParamCodes.Append(TEXT("else "));
		}
		SerializeParamCodes.Append(RPC->GetCode_SerializeFunctionParams());
		bIsFirst = false;
	}
	return SerializeParamCodes;
}

FString FReplicatedActorDecorator::GetCode_DeserializeFunctionParams()
{
	FString DeserializeParamCodes;
	bool bIsFirst = true;
	for (int32 i = 0; i < RPCs.Num(); i++)
	{
		const TSharedPtr<FRPCDecorator> RPC = RPCs[i];
		if (!bIsFirst)
		{
			DeserializeParamCodes.Append(TEXT("else "));
		}
		DeserializeParamCodes.Append(RPC->GetCode_DeserializeFunctionParams());
		bIsFirst = false;
	}
	return DeserializeParamCodes;
}

FString FReplicatedActorDecorator::GetDeclaration_RPCParamStructNamespace()
{
	return (GetReplicatorClassName(false) + TEXT("_rpcparamstruct")).ToLower();
}

FString FReplicatedActorDecorator::GetDeclaration_RPCParamStructs()
{
	FString RPCParamStructsDeclarations = TEXT("");

	if (GetRPCNum() > 0)
	{
		for (int32 i = 0; i < RPCs.Num(); i++)
		{
			const TSharedPtr<FRPCDecorator> RPC = RPCs[i];
			RPCParamStructsDeclarations.Append(RPC->GetDeclaration_PropPtrGroupStruct());
		}
	}
	return RPCParamStructsDeclarations;
}

FString FReplicatedActorDecorator::GetInstanceRefName() const
{
	return InstanceRefName;
}

void FReplicatedActorDecorator::SetInstanceRefName(const FString& InInstanceRefName)
{
	this->InstanceRefName = InInstanceRefName;
}

FString FReplicatedActorDecorator::GetCode_GetWorldRef()
{
	return InstanceRefName + TEXT("->GetWorld()");
}

FString FReplicatedActorDecorator::GetCode_OverrideGetNetGUID()
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_ReplicatorClassName"), GetReplicatorClassName());
	if (Target->IsChildOf(AGameStateBase::StaticClass()))
	{
		return FString::Format(GameState_GetNetGUIDTemplate, FormatArgs);
	}
	else if (Target->IsChildOf(UActorComponent::StaticClass()))
	{
		FormatArgs.Add(TEXT("Ref_TargetInstanceRef"), InstanceRefName);
		return FString::Format(ActorComp_GetNetGUIDTemplate, FormatArgs);
	}
	return TEXT("");
}
