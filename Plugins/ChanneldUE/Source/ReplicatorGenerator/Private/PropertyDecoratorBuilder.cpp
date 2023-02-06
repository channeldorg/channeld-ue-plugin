#include "PropertyDecoratorBuilder.h"

#include "PropertyDecorator/BaseDataTypePropertyDecorator.h"

TSharedPtr<FPropertyDecoratorBuilder> FPropertyDecoratorBuilder::SetNextBuilder(TSharedPtr<FPropertyDecoratorBuilder> InNextBuilder)
{
	this->NextBuilder = InNextBuilder;
	return NextBuilder;
}

FPropertyDecorator* FPropertyDecoratorBuilder::GetPropertyDecorator(FProperty* Property, IPropertyDecoratorOwner* Owner)
{
	if (IsSpecialProperty(Property))
	{
		return ConstructPropertyDecorator(Property, Owner);
	}
	else
	{
		return DoNext(Property, Owner);
	}
}

FPropertyDecorator* FPropertyDecoratorBuilder::DoNext(FProperty* Property, IPropertyDecoratorOwner* Owner)
{
	if (NextBuilder)
	{
		return NextBuilder->GetPropertyDecorator(Property, Owner);
	}
	else
	{
		return nullptr;
	}
}
