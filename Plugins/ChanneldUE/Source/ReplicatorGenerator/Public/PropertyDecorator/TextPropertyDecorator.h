#pragma once
#include "PropertyDecorator.h"

const static TCHAR* TextPropDeco_SetDeltaStateByMemOffsetTemp =
	LR"EOF(
{
  {Code_AssignPropPointers};
  if ({Code_BeforeCondition}{Declare_PropertyPtr}->EqualTo({Code_GetProtoFieldValue}))
  {
    {Code_SetProtoFieldValue};
    bStateChanged = true;
  }
}
)EOF";

const static TCHAR* TextPropDeco_SetDeltaStateArrayInnerTemp =
	LR"EOF(
std::string PropItem = std::string(TCHAR_TO_UTF8(*(*{Declare_PropertyPtr})[i].ToString()));
std::string* NewOne = {Declare_DeltaStateName}->add_{Definition_ProtoName}();
*NewOne = PropItem;
if (!bStateChanged)
{
  bStateChanged = !(PropItem == {Declare_FullStateName}->{Definition_ProtoName}()[i]);
}
)EOF";

const static TCHAR* TextPropDeco_OnChangeStateByMemOffsetTemp =
	LR"EOF(
{
  {Code_AssignPropPointers};
  if ({Code_HasProtoFieldValue} && {Declare_PropertyPtr}->EqualTo({Code_GetProtoFieldValue}))
  {
    *{Declare_PropertyPtr} = {Code_GetProtoFieldValue};
    bStateChanged = true;
  }
}
)EOF";

const static TCHAR* TextPropDeco_OnChangeStateArrayInnerTemp =
	LR"EOF(
FText NewText = FText::FromString(UTF8_TO_TCHAR(MessageArr[i].c_str()));
if (!(*{Declare_PropertyPtr})[i].EqualTo(NewText))
{
  (*{Declare_PropertyPtr})[i] = NewText;
  if (!bStateChanged)
  {
    bStateChanged = true;
  }
}
)EOF";

class FTextPropertyDecorator : public FPropertyDecorator
{
public:
	FTextPropertyDecorator(FProperty* InProperty, IPropertyDecoratorOwner* InOwner)
		: FPropertyDecorator(InProperty, InOwner)
	{
		ProtoFieldType = TEXT("string");
	}
	virtual FString GetPropertyType() override;

	virtual FString GetCode_GetProtoFieldValueFrom(const FString& StateName) override;
	
	virtual FString GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode) override;
	
	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState) override;
	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState, bool ForceFromPointer) override;

	virtual FString GetCode_SetDeltaStateByMemOffset(const FString& ContainerName, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull) override;
	virtual FString GetCode_SetDeltaStateArrayInner(const FString& PropertyPointer, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull) override;

	virtual FString GetCode_OnStateChangeByMemOffset(const FString& ContainerName, const FString& NewStateName) override;
	virtual FString GetCode_SetPropertyValueArrayInner(const FString& PropertyPointer, const FString& NewStateName) override;
};
