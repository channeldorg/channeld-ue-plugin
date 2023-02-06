#pragma once
#include "PropertyDecoratorBuilder.h"

class FVectorPropertyDecoratorBuilder: public FPropertyDecoratorBuilder
{
public:
	virtual ~FVectorPropertyDecoratorBuilder() override {};
	virtual bool IsSpecialProperty(FProperty* Property) override;

protected:
	virtual FPropertyDecorator* ConstructPropertyDecorator(FProperty* Property, IPropertyDecoratorOwner* InOwner) override;
};