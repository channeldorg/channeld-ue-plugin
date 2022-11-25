#include "PropertyDecoratorFactory.h"

#include "PropertyDecoratorBuilder/BaseDataTypePropertyDecoratorBuilder.h"

// Static Variables
FPropertyDecoratorFactory* FPropertyDecoratorFactory::Singleton;

FPropertyDecoratorFactory& FPropertyDecoratorFactory::Get()
{
	if (Singleton == nullptr)
	{
		Singleton = new FPropertyDecoratorFactory();
		Singleton->HeadBuilder = MakeShared<FUInt32PropertyDecoratorBuilder>();
		// HeadBuilder
		// 	->SetNextBuilder(MakeShared<FUInt64PropertyDecoratorBuilder>())
		// 	->SetNextBuilder(MakeShared<FFloatPropertyDecoratorBuilder>());
	}
	return *Singleton;
}

TSharedPtr<FPropertyDecorator> FPropertyDecoratorFactory::GetPropertyDecorator(FProperty* Property)
{
	return MakeShareable(HeadBuilder->GetPropertyDecorator(Property));
}
