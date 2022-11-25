#pragma once
#include "PropertyDecorator.h"
#include "PropertyDecoratorBuilder.h"
#include "Templates/SharedPointer.h"

class FPropertyDecoratorFactory
{
public:
	static FPropertyDecoratorFactory& Get();

	TSharedPtr<FPropertyDecorator> GetPropertyDecorator(FProperty*);

private:
	static FPropertyDecoratorFactory* Singleton;

	TSharedPtr<FPropertyDecoratorBuilder> HeadBuilder;
};
