#pragma once
#include "IPropertyDecoratorOwner.h"

class FPropertyDecorator;

class FPropertyDecoratorBuilder
{
public:
	virtual ~FPropertyDecoratorBuilder() = default;

	virtual TSharedPtr<FPropertyDecoratorBuilder> SetNextBuilder(TSharedPtr<FPropertyDecoratorBuilder>);

	virtual bool IsSpecialProperty(FProperty*) = 0;
	
	virtual FPropertyDecorator* GetPropertyDecorator(FProperty*, IPropertyDecoratorOwner*);

protected:
	TSharedPtr<FPropertyDecoratorBuilder> NextBuilder;
	virtual FPropertyDecorator* ConstructPropertyDecorator(FProperty*, IPropertyDecoratorOwner*) = 0;
	virtual FPropertyDecorator* DoNext(FProperty*, IPropertyDecoratorOwner*);

};

