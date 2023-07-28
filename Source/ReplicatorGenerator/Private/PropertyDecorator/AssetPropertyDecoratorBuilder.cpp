#include "PropertyDecorator/AssetPropertyDecoratorBuilder.h"

#include "PropertyDecorator/AssetPropertyDecorator.h"

bool FAssetPropertyDecoratorBuilder::IsSpecialProperty(FProperty* Property)
{
	if (Property->IsA<FObjectProperty>())
	{
		return Property->GetUPropertyWrapper()->GetClass()->ImplementsInterface(UInterface_AssetUserData::StaticClass());
	}
	return false;
}

FPropertyDecorator* FAssetPropertyDecoratorBuilder::ConstructPropertyDecorator(FProperty* Property, IPropertyDecoratorOwner* InOwner)
{
	return new FAssetPropertyDecorator(Property, InOwner);
}
