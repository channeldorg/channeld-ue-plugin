#include "PropertyDecoratorFactory.h"

#include "PropertyDecorator/BaseDataTypePropertyDecorator.h"
#include "PropertyDecorator/StructPropertyDecoratorBuilder.h"

// Static singleton
FPropertyDecoratorFactory* FPropertyDecoratorFactory::Singleton;

FPropertyDecoratorFactory& FPropertyDecoratorFactory::Get()
{
	if (Singleton == nullptr)
	{
		Singleton = new FPropertyDecoratorFactory();
		Singleton->HeadBuilder = MakeShared<FBytePropertyDecoratorBuilder>();
		Singleton->StructBuilder = MakeShared<FStructPropertyDecoratorBuilder>();
		Singleton->HeadBuilder
		         ->SetNextBuilder(MakeShared<FBoolPropertyDecoratorBuilder>())
		         ->SetNextBuilder(MakeShared<FUInt32PropertyDecoratorBuilder>())
		         ->SetNextBuilder(MakeShared<FIntPropertyDecoratorBuilder>())
		         ->SetNextBuilder(MakeShared<FUInt64PropertyDecoratorBuilder>())
		         ->SetNextBuilder(MakeShared<FInt64PropertyDecoratorBuilder>())
		         ->SetNextBuilder(MakeShared<FFloatPropertyDecoratorBuilder>())
		         ->SetNextBuilder(MakeShared<FDoublePropertyDecoratorBuilder>())
		         ->SetNextBuilder(MakeShared<FStrPropertyDecoratorBuilder>())
		         ->SetNextBuilder(MakeShared<FTextPropertyDecoratorBuilder>())
		         ->SetNextBuilder(MakeShared<FNamePropertyDecoratorBuilder>())
		         ->SetNextBuilder(MakeShared<FArrayPropertyDecoratorBuilder>())
		         ->SetNextBuilder(Singleton->StructBuilder);
	}
	return *Singleton;
}

TSharedPtr<FPropertyDecorator> FPropertyDecoratorFactory::GetPropertyDecorator(FProperty* Property, IPropertyDecoratorOwner* Owner)
{
	return MakeShareable(HeadBuilder->GetPropertyDecorator(Property, Owner));
}

TArray<TSharedPtr<FStructPropertyDecorator>> FPropertyDecoratorFactory::GetGlobalStructDecorators()
{
	return StructBuilder->GetGlobalStructs();
}

void FPropertyDecoratorFactory::ClearGlobalStruct()
{
	StructBuilder->ClearGlobalStructs();
}
