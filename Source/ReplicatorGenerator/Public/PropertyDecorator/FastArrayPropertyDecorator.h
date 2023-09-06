#pragma once
#include "PropertyDecorator.h"


class FFastArrayPropertyDecorator : public FPropertyDecorator
{
public:
	FFastArrayPropertyDecorator(FProperty* InProperty, IPropertyDecoratorOwner* InOwner)
		: FPropertyDecorator(InProperty, InOwner)
	{
		ProtoFieldType = TEXT("bytes");
		SetForceNotDirectlyAccessible(true);
	}
	virtual FString GetPropertyType() override;
	virtual FString GetPropertyName();

	virtual FString GetCode_OnStateChange(const FString& TargetInstanceName, const FString& NewStateName, const FString& AfterSetValueCode) override;
	virtual FString GetCode_GetProtoFieldValueFrom(const FString& StateName) override;
	FString GetCode_SetDeltaState(const FString& TargetInstance, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull) override;
	virtual FString GetDeclaration_PropertyPtr() override;
	virtual FString GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode) override;
	virtual TArray<FString> GetAdditionalIncludes() override;
	virtual FString GetCode_HasProtoFieldValueIn(const FString& StateName) override;

	FString GetLastFullNetStateName();
	FString GetDeltaUpdateCountName();
	FString GetFullUpdateProtoFieldName();

	FString GetProtoFieldsDefinition(int* NextIndex) override;

};
