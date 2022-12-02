#include "PropertyDecoratorBuilder.h"

#include "PropertyDecorator/BaseDataTypePropertyDecorator.h"

TSharedPtr<FPropertyDecoratorBuilder> FPropertyDecoratorBuilder::SetNextBuilder(TSharedPtr<FPropertyDecoratorBuilder> InNextBuilder)
{
	this->NextBuilder = InNextBuilder;
	return NextBuilder;
}

FPropertyDecorator* FPropertyDecoratorBuilder::GetPropertyDecorator(FProperty* Property)
{
	if (IsSpecialProperty(Property))
	{
		return ConstructPropertyDecorator(Property);
	}
	else
	{
		return DoNext(Property);
	}
}

FPropertyDecorator* FPropertyDecoratorBuilder::DoNext(FProperty* Property)
{
	if (NextBuilder)
	{
		return NextBuilder->GetPropertyDecorator(Property);
	}
	else
	{
		return nullptr;
	}
}
