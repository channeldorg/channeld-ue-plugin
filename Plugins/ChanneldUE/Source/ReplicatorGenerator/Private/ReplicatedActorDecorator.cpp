#include "ReplicatedActorDecorator.h"

#include "PropertyDecoratorFactory.h"

FReplicatedActorDecorator::FReplicatedActorDecorator(const UClass* TargetActorClass)
{
	Target = TargetActorClass;
	bIsBlueprintGenerated = Target->GetClass()->IsChildOf(UBlueprintGeneratedClass::StaticClass());
}

void FReplicatedActorDecorator::Init(const FModuleInfo& InModuleBelongTo)
{
	ModuleBelongTo = FModuleInfo(InModuleBelongTo);
	
	FPropertyDecoratorFactory& PropertyDecoratorFactory = FPropertyDecoratorFactory::Get();
	for (TFieldIterator<FProperty> It(Target); It; ++It)
	{
		FProperty* Property = *It;
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

FString FReplicatedActorDecorator::GetActorCppClassName()
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

FString FReplicatedActorDecorator::GetCode_AllPropertiesSetDeltaState(const FString& TargetInstanceRef, const FString& FullStateRef, const FString& DeltaStateRef)
{
	FStringBuilderBase SetDeltaStateCodeBuilder;
	for (const TSharedPtr<FPropertyDecorator> Property : Properties)
	{
		SetDeltaStateCodeBuilder.Append(Property->GetCode_SetDeltaState(TargetInstanceRef, FullStateRef, DeltaStateRef));
	}
	return SetDeltaStateCodeBuilder.ToString();
}

FString FReplicatedActorDecorator::GetCode_AllPropertiesOnStateChange(const FString& TargetInstanceRef, const FString& NewStateRef)
{
	FStringBuilderBase OnChangeStateCodeBuilder;
	for (const TSharedPtr<FPropertyDecorator> Property : Properties)
	{
		OnChangeStateCodeBuilder.Append(Property->GetCode_OnStateChange(TargetInstanceRef, NewStateRef));
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
	return  bIsBlueprintGenerated;
}
