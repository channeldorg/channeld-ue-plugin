#include "PropertyDecorator/VectorPropertyDecoratorBuilder.h"

#include "PropertyDecorator/VectorPropertyDecorator.h"

bool FVectorPropertyDecoratorBuilder::IsSpecialProperty(FProperty* Property)
{
	if (Property->IsA<FStructProperty>())
	{
		const FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(Property);
		const FString StructName = StructProperty->Struct->GetStructCPPName();
		if (StructName.Equals(TEXT("FVector")) || StructName.StartsWith(TEXT("FVector_NetQuantize")))
		{
			return true;
		}
	}
	return false;
}

FPropertyDecorator* FVectorPropertyDecoratorBuilder::ConstructPropertyDecorator(FProperty* Property, IPropertyDecoratorOwner* InOwner)
{
	return new FVectorPropertyDecorator(Property, InOwner);
}
