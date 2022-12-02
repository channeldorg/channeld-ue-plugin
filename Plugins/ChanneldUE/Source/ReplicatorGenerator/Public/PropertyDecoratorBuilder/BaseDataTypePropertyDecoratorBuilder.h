#pragma once
#include "PropertyDecoratorBuilder.h"

#define BASE_DATA_TYPE_PROPERTY_DECORATOR_BUILDER(ClassName, PropertyDecorator, PropertyType) \
class ClassName: public FPropertyDecoratorBuilder \
{ \
public: \
	virtual ~ClassName() override {}; \
	virtual bool IsSpecialProperty(FProperty* Property) override \
	{ \
		return Property->IsA<PropertyType>(); \
	} \
	virtual FPropertyDecorator* ConstructPropertyDecorator(FProperty* Property) override \
	{ \
		return new PropertyDecorator(); \
	} \
}

// class FUInt32PropertyDecoratorBuilder: public FPropertyDecoratorBuilder
// {
// public:
// 	virtual ~FUInt32PropertyDecoratorBuilder() override {};
// 	
// 	virtual bool IsSpecialProperty(FProperty* Property) override
// 	{
// 		return Property->IsA<FUInt32Property>();
// 	}
// 	
// 	virtual FPropertyDecorator* ConstructPropertyDecorator(FProperty* Property) override
// 	{
// 		return new FUInt32PropertyDecorator();
// 	}
// 	
// };
