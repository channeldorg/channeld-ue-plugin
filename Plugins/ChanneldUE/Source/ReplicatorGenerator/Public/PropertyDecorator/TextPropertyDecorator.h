#pragma once
#include "PropertyDecorator.h"

class FTextPropertyDecorator : public FPropertyDecorator
{
public:
	FTextPropertyDecorator(FProperty* InProperty, IPropertyDecoratorOwner* InOwner)
		: FPropertyDecorator(InProperty, InOwner)
	{
		ProtoFieldType = TEXT("string");
	}
	
	virtual FString GetCode_GetProtoFieldValueFrom(const FString& StateName) override;
	virtual FString GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode) override;
	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState) override;
	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState, bool ForceFromPointer) override;
	virtual FString GetPropertyType() override;
};
