#include "PropertyDecorator/ActorCompPropDecoratorBuilder.h"

#include "PropertyDecorator/ActorCompPropertyDecorator.h"

bool FActorCompPropDecoratorBuilder::IsSpecialProperty(FProperty* Property)
{
	if(Property->IsA<FObjectProperty>())
	{
		return CastFieldChecked<FObjectProperty>(Property)->PropertyClass->IsChildOf(UActorComponent::StaticClass());
	}
	return false;
}

FPropertyDecorator* FActorCompPropDecoratorBuilder::ConstructPropertyDecorator(FProperty* Property, IPropertyDecoratorOwner* InOwner)
{
	return new FActorCompPropertyDecorator(Property, InOwner);
}
