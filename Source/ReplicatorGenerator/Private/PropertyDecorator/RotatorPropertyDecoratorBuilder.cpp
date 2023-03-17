#include "PropertyDecorator/RotatorPropertyDecoratorBuilder.h"

#include "PropertyDecorator/RotatorPropertyDecorator.h"

bool FRotatorPropertyDecoratorBuilder::IsSpecialProperty(FProperty* Property)
{
	if (Property->IsA<FStructProperty>())
	{
		const FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(Property);
		if (StructProperty->Struct->GetStructCPPName().Equals(TEXT("FRotator")))
		{
			return true;
		}
	}
	return false;
}

FPropertyDecorator* FRotatorPropertyDecoratorBuilder::ConstructPropertyDecorator(FProperty* Property, IPropertyDecoratorOwner* InOwner)
{
	return new FRotatorPropertyDecorator(Property, InOwner);
}
