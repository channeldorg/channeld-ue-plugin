#include "PropertyDecorator/ClassPropertyDecoratorBuilder.h"
#include "PropertyDecorator/ClassPropertyDecorator.h"

bool FClassPropertyDecoratorBuilder::IsSpecialProperty(FProperty* Property)
{
	return Property->GetCPPType().StartsWith("TSubclassOf<");
}

FPropertyDecorator* FClassPropertyDecoratorBuilder::ConstructPropertyDecorator(FProperty* Property, IPropertyDecoratorOwner* InOwner)
{
	return new FClassPropertyDecorator(Property, InOwner);
}
