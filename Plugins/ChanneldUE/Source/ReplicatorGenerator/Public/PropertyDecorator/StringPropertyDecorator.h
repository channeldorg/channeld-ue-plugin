#pragma once
#include "PropertyDecorator.h"

const static TCHAR* StrPropDeco_SetDeltaStateArrayInnerTemp =
	LR"EOF(
std::string PropItem = std::string(TCHAR_TO_UTF8(*(*{Declare_PropertyPtr})[i]));
std::string* NewOne = {Declare_DeltaStateName}->add_{Definition_ProtoName}();
*NewOne = PropItem;
if (!bStateChanged)
{
  bStateChanged = !(PropItem == {Declare_FullStateName}->{Definition_ProtoName}()[i]);
}
)EOF";

const static TCHAR* StrPropDeco_OnChangeStateArrayInnerTemp =
	LR"EOF(
FString NewString = UTF8_TO_TCHAR(MessageArr[i].c_str());
if ((*{Declare_PropertyPtr})[i] != NewString)
{
  (*{Declare_PropertyPtr})[i] = NewString;
  if (!bStateChanged)
  {
    bStateChanged = true;
  }
}
)EOF";

class FStringPropertyDecorator : public FPropertyDecorator
{
public:
	FStringPropertyDecorator(FProperty* InProperty, IPropertyDecoratorOwner* InOwner)
		: FPropertyDecorator(InProperty, InOwner)
	{
		ProtoFieldType = TEXT("string");
	}
	virtual FString GetPropertyType() override;

	virtual FString GetCode_GetProtoFieldValueFrom(const FString& StateName) override;

	virtual FString GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode) override;

	virtual FString GetCode_SetDeltaStateArrayInner(const FString& PropertyPointer, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull) override;

	virtual FString GetCode_SetPropertyValueArrayInner(const FString& PropertyPointer, const FString& NewStateName) override;
};
