#pragma once
#include "PropertyDecorator.h"
#include "PropertyDecoratorBuilder.h"
#include "PropertyDecorator/StructPropertyDecoratorBuilder.h"
#include "Templates/SharedPointer.h"

class FPropertyDecoratorFactory
{
public:
	static FPropertyDecoratorFactory& Get();

	TSharedPtr<FPropertyDecorator> GetPropertyDecorator(FProperty*, IPropertyDecoratorOwner*);

	TArray<TSharedPtr<FStructPropertyDecorator>> GetGlobalStructDecorators();

private:
	static FPropertyDecoratorFactory* Singleton;

	TSharedPtr<FPropertyDecoratorBuilder> HeadBuilder;
	
	TSharedPtr<FStructPropertyDecoratorBuilder> StructBuilder;
};
