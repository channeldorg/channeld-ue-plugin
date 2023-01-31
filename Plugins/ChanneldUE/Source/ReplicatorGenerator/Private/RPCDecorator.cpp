#include "RPCDecorator.h"

#include "PropertyDecoratorFactory.h"
#include "ReplicatedActorDecorator.h"
#include "ReplicatorGeneratorUtils.h"

FRPCDecorator::FRPCDecorator(UFunction* InFunc, IPropertyDecoratorOwner* InOwner)
	: FStructPropertyDecorator(nullptr, InOwner), OriginalFunction(InFunc)
{
	if (OriginalFunction != nullptr)
	{
		FPropertyDecoratorFactory& PropertyDecoratorFactory = FPropertyDecoratorFactory::Get();
		for (TFieldIterator<FProperty> SIt(OriginalFunction, EFieldIteratorFlags::ExcludeSuper); SIt; ++SIt)
		{
			TSharedPtr<FPropertyDecorator> PropertyDecoratorPtr = PropertyDecoratorFactory.GetPropertyDecorator(*SIt, this);
			if (PropertyDecoratorPtr.IsValid())
			{
				PropertyDecoratorPtr->SetForceNotDirectlyAccessible(true);
				Properties.Add(PropertyDecoratorPtr);
			}
		}
	}
	OwnerActor = static_cast<FReplicatedActorDecorator*>(InOwner);
}

bool FRPCDecorator::Init(const TFunction<FString()>& SetNameForIllegalPropName)
{
	if (bInitialized)
	{
		return false;
	}

	CompilablePropName = OriginalFunction->GetName();
	if (!ChanneldReplicatorGeneratorUtils::IsCompilableClassName(CompilablePropName))
	{
		CompilablePropName = *SetNameForIllegalPropName();
	}

	PostInit();
	bInitialized = true;
	return true;
}

bool FRPCDecorator::IsDirectlyAccessible()
{
	return false;
}

FString FRPCDecorator::GetCPPType()
{
	return FString::Printf(TEXT("%sParamStruct"), *GetPropertyName());
}

FString FRPCDecorator::GetProtoPackageName()
{
	return Owner->GetProtoPackageName();
}

FString FRPCDecorator::GetProtoNamespace()
{
	return Owner->GetProtoNamespace();
}

FString FRPCDecorator::GetProtoStateMessageType()
{
	return FString::Printf(TEXT("Func%sParams"), *GetPropertyName());
}

FString FRPCDecorator::GetCode_SerializeFunctionParams()
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_FuncName"), OriginalFunction->GetName());
	FormatArgs.Add(TEXT("Declare_ParamStructNamespace"), OwnerActor->GetDeclaration_RPCParamStructNamespace());
	FormatArgs.Add(TEXT("Declare_PropPtrGroupStructName"), GetDeclaration_PropPtrGroupStructName());
	FormatArgs.Add(TEXT("Declare_ProtoNamespace"), GetProtoNamespace());
	FormatArgs.Add(TEXT("Declare_ProtoStateMsgName"), GetProtoStateMessageType());
	FormatArgs.Add(TEXT("Code_GetWorldRef"), Owner->GetCode_GetWorldRef());
	return FString::Format(RPC_SerializeFuncParamsTemp, FormatArgs);
}

FString FRPCDecorator::GetCode_DeserializeFunctionParams()
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_FuncName"), OriginalFunction->GetName());
	FormatArgs.Add(TEXT("Declare_ParamStructNamespace"), OwnerActor->GetDeclaration_RPCParamStructNamespace());
	FormatArgs.Add(TEXT("Declare_PropPtrGroupStructName"), GetDeclaration_PropPtrGroupStructName());
	FormatArgs.Add(TEXT("Declare_ProtoNamespace"), GetProtoNamespace());
	FormatArgs.Add(TEXT("Declare_ProtoStateMsgName"), GetProtoStateMessageType());
	FormatArgs.Add(TEXT("Declare_ParamStructCopy"), GetCompilableCPPType());
	FormatArgs.Add(TEXT("Code_GetWorldRef"), Owner->GetCode_GetWorldRef());
	return FString::Format(RPC_DeserializeFuncParamsTemp, FormatArgs);
}

FString FRPCDecorator::GetDeclaration_ProtoFields()
{
	FString FieldDefinitions;
	for (int32 i = 0; i < Properties.Num(); i++)
	{
		const TSharedPtr<FPropertyDecorator> Property = Properties[i];
		FieldDefinitions += Property->GetDefinition_ProtoField(i + 1) + TEXT(";\n");
	}
	return FieldDefinitions;
}

TArray<FString> FRPCDecorator::GetAdditionalIncludes()
{
	TSet<FString> IncludeFileSet;
	for (auto PropDecorator : Properties)
	{
		IncludeFileSet.Append(PropDecorator->GetAdditionalIncludes());
	}
	return IncludeFileSet.Array();
}

FString FRPCDecorator::GetCode_GetWorldRef()
{
	return TEXT("World");
}
