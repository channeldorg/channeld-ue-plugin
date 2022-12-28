#pragma once
#include "PropertyDecorator.h"

class FVectorPropertyDecorator : public FPropertyDecorator
{
public:
	FVectorPropertyDecorator(FProperty* InProperty, IPropertyDecoratorOwner* InOwner)
		: FPropertyDecorator(InProperty, InOwner)
	{
	}

	virtual ~FVectorPropertyDecorator() override = default;

	virtual FString GetPropertyType() override;

	virtual FString GetProtoFieldType() override;

	virtual FString GetProtoPackageName() override;
	virtual FString GetProtoNamespace() override;
	virtual FString GetProtoStateMessageType() override;

	virtual FString GetCode_GetProtoFieldValueFrom(const FString& StateName) override;

	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState) override;

	virtual FString GetCode_SetDeltaState(const FString& TargetInstance, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull) override;
	virtual FString GetCode_SetDeltaStateByMemOffset(const FString& ContainerName, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull = false) override;

	virtual TArray<FString> GetAdditionalIncludes() override;
};
