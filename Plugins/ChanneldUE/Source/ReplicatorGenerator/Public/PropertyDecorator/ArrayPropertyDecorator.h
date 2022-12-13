#pragma once
#include "PropertyDecorator.h"
const static TCHAR* ArrPropDecorator_SetDeltaStateTemplate =
	LR"EOF(
{
  const int32 ActorPropLength = {Declare_PropPtrName}->Num();
  const int32 FullStateValueLength = {Code_GetProtoFieldValueFrom}.size();
  if (ActorPropLength != FullStateValueLength)
  {
    bStateChanged = true;
  }
  for (int32 i = 0; i < ActorPropLength; ++i)
  {
  {Code_SetDeltaStateArrayInner}
  }
}
)EOF";

const static TCHAR* ArrPropDecorator_SetPropertyValueTemp =
	LR"EOF(
const int32 ActorPropLength = {Declare_PropPtrName}->Num();
auto & MessageArr = {Code_GetProtoFieldValueFrom};
const int32 NewStateValueLength = MessageArr.size();
if (ActorPropLength != NewStateValueLength) { bStateChanged = true; }
{Declare_PropPtrName}->SetNum(NewStateValueLength);
for (int32 i = 0; i < NewStateValueLength; ++i)
{
{Code_SetPropertyValueArrayInner}
}
)EOF";

class FArrayPropertyDecorator : public FPropertyDecorator
{
public:
	FArrayPropertyDecorator(FProperty* InProperty, IPropertyDecoratorOwner* InOwner);

	virtual ~FArrayPropertyDecorator() override = default;
	
	virtual FString GetCPPType() override;
	
	virtual FString GetPropertyType() override;
	virtual FString GetCode_SetDeltaState(const FString& TargetInstance, const FString& FullStateName, const FString& DeltaStateName,  bool ConditionFullStateIsNull) override;

	virtual FString GetCode_HasProtoFieldValueIn(const FString& StateName) override;
	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState) override;
	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState, bool ForceFromPointer) override;

	virtual FString GetCode_SetPropertyValueTo(const FString& TargetInstance, const FString& NewStataName, const FString& AfterSetValueCode) override;
protected:
	TSharedPtr<FPropertyDecorator> InnerProperty;
};
