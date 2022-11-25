#pragma once
#include "PropertyDecoratorBuilder.h"
#include "PropertyDecorator/BaseDataTypePropertyDecorator.h"

class FUInt32PropertyDecoratorBuilder: public FPropertyDecoratorBuilder
{
public:
	virtual ~FUInt32PropertyDecoratorBuilder() override {};
	
	virtual bool IsSpecialTarget(FProperty* Property) override
	{
		return Property->IsA<FUInt32Property>();
	}
	
	virtual FPropertyDecorator* ConstructPropertyDecorator(FProperty* Property) override
	{
		return new FUInt32PropertyDecorator();
	}
	
};

