#include "PropertyDecorator/FastArrayPropertyDecoratorBuilder.h"

#include "PropertyDecorator/FastArrayPropertyDecorator.h"

bool FFastArrayPropertyDecoratorBuilder::IsSpecialProperty(FProperty* Property)
{
	if (Property->IsA<FStructProperty>())
	{
		const FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(Property);
		if (StructProperty && StructProperty->Struct && StructProperty->Struct->GetCppStructOps() && StructProperty->
			Struct->GetCppStructOps()->HasNetDeltaSerializer())
		{
			return true;
		}
	}
	return false;
}

FPropertyDecorator* FFastArrayPropertyDecoratorBuilder::ConstructPropertyDecorator(FProperty* Property, IPropertyDecoratorOwner* InOwner)
{
	return new FFastArrayPropertyDecorator(Property, InOwner);
}