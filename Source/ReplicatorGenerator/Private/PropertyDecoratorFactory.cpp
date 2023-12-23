#include "PropertyDecoratorFactory.h"

#include "PropertyDecorator/ActorCompPropDecoratorBuilder.h"
#include "PropertyDecorator/AssetPropertyDecoratorBuilder.h"
#include "PropertyDecorator/BaseDataTypePropertyDecorator.h"
#include "PropertyDecorator/RotatorPropertyDecoratorBuilder.h"
#include "PropertyDecorator/VectorPropertyDecoratorBuilder.h"
#include "PropertyDecorator/ClassPropertyDecoratorBuilder.h"

FPropertyDecoratorFactory::FPropertyDecoratorFactory()
{
	HeadBuilder = MakeShared<FBytePropertyDecoratorBuilder>();
	HeadBuilder
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
		->SetNextBuilder(MakeShared<FClassPropertyDecoratorBuilder>())
		->SetNextBuilder(MakeShared<FArrayPropertyDecoratorBuilder>())
		->SetNextBuilder(MakeShared<FVectorPropertyDecoratorBuilder>())
		->SetNextBuilder(MakeShared<FRotatorPropertyDecoratorBuilder>())
		->SetNextBuilder(MakeShared<FStructPropertyDecoratorBuilder>())
		->SetNextBuilder(MakeShared<FActorCompPropDecoratorBuilder>())
		->SetNextBuilder(MakeShared<FAssetPropertyDecoratorBuilder>())
		->SetNextBuilder(MakeShared<FUObjPropertyDecoratorBuilder>());
}

FPropertyDecoratorFactory& FPropertyDecoratorFactory::Get()
{
	static FPropertyDecoratorFactory Singleton;
	return Singleton;
}

TSharedPtr<FPropertyDecorator> FPropertyDecoratorFactory::GetPropertyDecorator(FProperty* Property, IPropertyDecoratorOwner* Owner)
{
	return MakeShareable(HeadBuilder->GetPropertyDecorator(Property, Owner));
}
