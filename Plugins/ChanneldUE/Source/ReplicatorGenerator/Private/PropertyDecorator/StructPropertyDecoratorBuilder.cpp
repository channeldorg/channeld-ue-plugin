#include "PropertyDecorator/StructPropertyDecoratorBuilder.h"

TArray<TSharedPtr<FStructPropertyDecorator>> FStructPropertyDecoratorBuilder::GetGlobalStructs()
{
	TArray<TSharedPtr<FStructPropertyDecorator>> Result;
	GlobalStructs.GenerateValueArray(Result);
	return Result;
}

FPropertyDecorator* FStructPropertyDecoratorBuilder::ConstructPropertyDecorator(FProperty* Property, IPropertyDecoratorOwner* InOwner)
{
	const FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(Property);
	UScriptStruct* ScriptStruct = StructProperty->Struct;
	if (!GlobalStructs.Contains(ScriptStruct))
	{
		GlobalStructs.Add(ScriptStruct, MakeShareable(new FStructPropertyDecorator(Property, InOwner)));
	}
	return new FStructPropertyDecorator(Property, InOwner);
}
