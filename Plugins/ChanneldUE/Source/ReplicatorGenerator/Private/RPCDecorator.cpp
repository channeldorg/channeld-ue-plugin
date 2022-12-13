#include "RPCDecorator.h"

#include "PropertyDecoratorFactory.h"

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
				Properties.Add(PropertyDecoratorPtr);
			}
		}
	}
}

bool FRPCDecorator::IsBlueprintType()
{
	return false;
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
	return FString::Printf(TEXT("Func%sParams"), *OriginalFunction->GetName());
}

FString FRPCDecorator::GetCode_SerializeFunctionParams()
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_Name"), OriginalFunction->GetName());
	FormatArgs.Add(TEXT("Declare_PropPtrGroupStructName"), GetDeclaration_PropPtrGroupStructName());
	FormatArgs.Add(TEXT("Declare_ProtoNamespace"), GetProtoNamespace());
	FormatArgs.Add(TEXT("Declare_ProtoStateMsgName"), GetProtoStateMessageType());

	return FString::Format(StructPropDeco_AssignPropPtrOrderlyTemp, FormatArgs);
}
