#include "PropertyDecorator/AssetPropertyDecoratorBuilder.h"

#include "GameFramework/WorldSettings.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PropertyDecorator/AssetPropertyDecorator.h"

bool FAssetPropertyDecoratorBuilder::IsSpecialProperty(FProperty* Property)
{
	if (Property->IsA<FObjectProperty>())
	{
		const UClass* PropertyClass = CastFieldChecked<FObjectProperty>(Property)->PropertyClass;
		return PropertyClass->IsAsset() || PropertyClass->IsChildOf<UPhysicsAsset>() ||
			(PropertyClass->ImplementsInterface(UInterface_AssetUserData::StaticClass())
				&& !PropertyClass->IsChildOf<UActorComponent>()
				&& !PropertyClass->IsChildOf<AWorldSettings>()
				&& !PropertyClass->IsChildOf<ULevel>());
	}
	return false;
}

FPropertyDecorator* FAssetPropertyDecoratorBuilder::ConstructPropertyDecorator(FProperty* Property, IPropertyDecoratorOwner* InOwner)
{
	return new FAssetPropertyDecorator(Property, InOwner);
}
