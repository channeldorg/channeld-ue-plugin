#include "PropertyDecorator/VectorPropertyDecoratorBuilder.h"

#include "PropertyDecorator/VectorPropertyDecorator.h"

bool FVectorPropertyDecoratorBuilder::IsSpecialProperty(FProperty* Property)
{
	if (Property->IsA<FStructProperty>())
	{
		const FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(Property);
		if (StructProperty->Struct->GetStructCPPName().Equals(TEXT("FVector")))
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
