#pragma once
#include "PropertyDecorator/VectorPropertyDecorator.h"

class FRotatorPropertyDecorator : public FVectorPropertyDecorator
{
public:
	FRotatorPropertyDecorator(FProperty* InProperty, IPropertyDecoratorOwner* InOwner)
		: FVectorPropertyDecorator(InProperty, InOwner)
	{
	}

	virtual FString GetPropertyType() override;
	virtual FString GetCode_GetProtoFieldValueFrom(const FString& StateName) override;
};
