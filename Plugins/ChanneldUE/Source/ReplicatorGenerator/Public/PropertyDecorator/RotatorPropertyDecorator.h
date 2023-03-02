#pragma once
#include "PropertyDecorator/VectorPropertyDecorator.h"

const static TCHAR* RotatorPropDeco_SetDeltaStateArrayInnerTemp =
	LR"EOF(
FRotator& PropItem = (*{Declare_PropertyPtr})[i];
unrealpb::FVector* NewOne = {Declare_DeltaStateName}->add_{Definition_ProtoName}();
ChanneldUtils::SetRotatorToPB(NewOne, PropItem);
if (!bPropChanged)
{
  bPropChanged = !(PropItem == ChanneldUtils::GetRotator({Declare_FullStateName}->{Definition_ProtoName}()[i]));
}
)EOF";

const static TCHAR* RotatorPropDeco_OnChangeStateArrayInnerTemp =
	LR"EOF(
FRotator NewVector = ChanneldUtils::GetRotator(MessageArr[i]);
if ((*{Declare_PropertyPtr})[i] != NewVector)
{
  (*{Declare_PropertyPtr})[i] = NewVector;
  if (!bPropChanged)
  {
    bPropChanged = true;
  }
}
)EOF";

class FRotatorPropertyDecorator : public FVectorPropertyDecorator
{
public:
	FRotatorPropertyDecorator(FProperty* InProperty, IPropertyDecoratorOwner* InOwner)
		: FVectorPropertyDecorator(InProperty, InOwner)
	{
	}
	virtual FString GetPropertyType() override;

	virtual FString GetFunctionName_SetXXXFromPB() const override;
	virtual FString GetFunctionName_SetXXXToPB() const override;


};
