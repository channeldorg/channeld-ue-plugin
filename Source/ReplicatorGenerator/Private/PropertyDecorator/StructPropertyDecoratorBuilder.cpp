#include "PropertyDecorator/StructPropertyDecoratorBuilder.h"

void FStructPropertyDecoratorBuilder::AddReferencedObjects(FReferenceCollector& Collector)
{
	TArray<UScriptStruct*> GlobalStructArray;
	GlobalStructs.GetKeys(GlobalStructArray);
	Collector.AddReferencedObjects(GlobalStructArray);
}

TArray<TSharedPtr<FStructPropertyDecorator>> FStructPropertyDecoratorBuilder::GetAndInitGlobalStructs()
{
	TArray<TSharedPtr<FStructPropertyDecorator>> Result;
	GlobalStructs.GenerateValueArray(Result);
	int32 IllegalPropNameIndex = 0;
	for (TSharedPtr<FStructPropertyDecorator> StructDecorator : Result)
	{
		StructDecorator->Init(
			[&IllegalPropNameIndex]()
			{
				return FString::Printf(TEXT("_IllegalNameProp_%d_"), ++IllegalPropNameIndex);
			}
		);
	}
	return Result;
}

void FStructPropertyDecoratorBuilder::ClearGlobalStructs()
{
	GlobalStructs.Empty();
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
