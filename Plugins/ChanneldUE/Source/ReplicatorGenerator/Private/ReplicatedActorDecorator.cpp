#include "ReplicatedActorDecorator.h"

#include "PropertyDecoratorFactory.h"

FReplicatedActorDecorator::FReplicatedActorDecorator(const UClass* TargetActorClass)
{
	Target = TargetActorClass;
	bIsBlueprintGenerated = Target->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
}

void FReplicatedActorDecorator::Init()
{
	FPropertyDecoratorFactory& PropertyDecoratorFactory = FPropertyDecoratorFactory::Get();
	for (TFieldIterator<FProperty> It(Target); It; ++It)
	{
		FProperty* Property = *It;
		if(Property->Owner != Target)
		{
			continue;
		}
		if (Property->HasAnyPropertyFlags(CPF_Net))
		{
			TSharedPtr<FPropertyDecorator> PropertyDecoratorPtr = PropertyDecoratorFactory.GetPropertyDecorator(Property);
			if (PropertyDecoratorPtr.IsValid())
			{
				Properties.Emplace(PropertyDecoratorPtr);
				PropertyDecoratorPtr->Init(Property, this);
			}
		}
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
			Result += FString::Printf(TEXT("%s* %s;\n"), *Property->GetCPPType(), *Property->GetPointerName());
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
			FStringFormatNamedArguments FormatArgs;
			FormatArgs.Add(TEXT("Ref_TargetInstanceRef"), FStringFormatArg(TargetInstance));
			FormatArgs.Add(TEXT("Declare_PropertyType"), FStringFormatArg(Property->GetPropertyType()));
			FormatArgs.Add(TEXT("Declare_PropertyName"), FStringFormatArg(Property->GetPropertyName()));
			FormatArgs.Add(TEXT("Declare_PropertyPointer"), FStringFormatArg(Property->GetPointerName()));
			FormatArgs.Add(TEXT("Declare_PropertyCPPType"), FStringFormatArg(Property->GetCPPType()));
			
			Result += FString::Format(ReplicatedActorDeco_GetCode_AssignPropertyPointerTemplate, FormatArgs);
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
	if(Properties.Num() == 0)
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
	if(Properties.Num() == 0)
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

	return FString::Format(ReplicatedActorDeco_ProtoStateMessageTemplate, FormatArgs);
}

bool FReplicatedActorDecorator::IsBlueprintGenerated()
{
	return bIsBlueprintGenerated;
}
