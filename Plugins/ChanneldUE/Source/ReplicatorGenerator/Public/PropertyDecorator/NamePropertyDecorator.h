#pragma once
#include "PropertyDecorator.h"

class FNamePropertyDecorator : public FPropertyDecorator
{
public:
	FNamePropertyDecorator(FProperty* InProperty, IPropertyDecoratorOwner* InOwner)
	: FPropertyDecorator(InProperty, InOwner)
	{
		ProtoFieldType = TEXT("string");
	}
	
	virtual FString GetCode_GetProtoFieldValueFrom(const FString& StateName) override;
	virtual FString GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode) override;

	virtual FString GetPropertyType() override;
};
