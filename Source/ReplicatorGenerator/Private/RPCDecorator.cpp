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
	// The generated params struct name will be added to 'ChanneldGlobalStruct.h'.
	// Add ActorNameHash to avoid name conflict.
	CompilablePropName = FString::Printf(TEXT("%s_%s"), *CompilablePropName, *ChanneldReplicatorGeneratorUtils::GetHashString(OwnerActor->GetActorPathName()));

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
	return FString::Printf(TEXT("RPCParamStruct%s"), *GetPropertyName());
}

FString FRPCDecorator::GetProtoStateMessageType()
{
	return FString::Printf(TEXT("RPCParams%s"), *GetPropertyName());
}

FString FRPCDecorator::GetCode_SerializeFunctionParams()
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_FuncName"), OriginalFunction->GetName());
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

FString FRPCDecorator::GetCode_GetWorldRef()
{
	return TEXT("World");
}
