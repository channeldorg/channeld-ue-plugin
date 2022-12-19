#pragma once
#include "PropertyDecorator.h"

class FStringPropertyDecorator : public FPropertyDecorator
{
public:
	FStringPropertyDecorator(FProperty* InProperty, IPropertyDecoratorOwner* InOwner)
		: FPropertyDecorator(InProperty, InOwner)
	{
		ProtoFieldType = TEXT("string");
	}

	virtual FString GetCode_GetProtoFieldValueFrom(const FString& StateName) override;
	virtual FString GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode) override;

	virtual FString GetPropertyType() override;
};
