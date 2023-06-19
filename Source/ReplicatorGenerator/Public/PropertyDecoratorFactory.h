#pragma once
#include "PropertyDecorator.h"
#include "PropertyDecoratorBuilder.h"
#include "Templates/SharedPointer.h"

class FPropertyDecoratorFactory
{
private:
	FPropertyDecoratorFactory();
	~FPropertyDecoratorFactory() = default;
	FPropertyDecoratorFactory(const FPropertyDecoratorFactory&) = delete;
	FPropertyDecoratorFactory(const FPropertyDecoratorFactory&&) = delete;
	FPropertyDecoratorFactory& operator=(const FPropertyDecoratorFactory&) = delete;

	TSharedPtr<FPropertyDecoratorBuilder> HeadBuilder;

public:
	static FPropertyDecoratorFactory& Get();

	TSharedPtr<FPropertyDecorator> GetPropertyDecorator(FProperty*, IPropertyDecoratorOwner*);
};
