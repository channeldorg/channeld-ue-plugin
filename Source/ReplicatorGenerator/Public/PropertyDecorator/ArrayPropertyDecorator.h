#pragma once
#include "PropertyDecorator.h"
const static TCHAR* ArrPropDeco_SetDeltaStateTemplate =
	LR"EOF(
bool b{Declare_PropertyName}Changed = false;	//	ArrPropDeco_SetDeltaStateTemplate
{
  bool bPropChanged = false;
  const int32 ActorPropLength = {Declare_PropPtrName}->Num();
  const int32 FullStateValueLength = {Code_ConditionFullStateIsNull}{Code_GetProtoFieldValueFrom}.size();
  if (ActorPropLength != FullStateValueLength)
  {
    bPropChanged = true;
  }
  for (int32 i = 0; i < ActorPropLength; ++i)
  {
  {Code_SetDeltaStateArrayInner}
  }
  if (bPropChanged)
  {
	b{Declare_PropertyName}Changed = true;	//	ArrPropDeco_SetDeltaStateTemplate
    {Declare_DeltaStateName}->set_update_{Definition_ProtoName}(true);
    if({Declare_FullStateName} != nullptr && {Declare_FullStateName}->{Definition_ProtoName}_size() > 0)
    {
      const_cast<{Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}*>({Declare_FullStateName})->clear_{Definition_ProtoName}();
    }
  }
  else
  {
    {Declare_DeltaStateName}->clear_{Definition_ProtoName}();
  }
}
bStateChanged |= b{Declare_PropertyName}Changed;	//	ArrPropDeco_SetDeltaStateTemplate
)EOF";

const static TCHAR* ArrPropDeco_SetDeltaStateByMemOffsetTemp =
	LR"EOF(
bool b{Declare_PropertyName}Changed = false;	//	ArrPropDeco_SetDeltaStateByMemOffsetTemp
{
  bool bPropChanged = false;
  {Code_AssignPropPointers};
  const int32 ActorPropLength = PropAddr->Num();
  const int32 FullStateValueLength = {Code_ConditionFullStateIsNull}{Code_GetProtoFieldValueFrom}.size();
  if (ActorPropLength != FullStateValueLength)
  {
    bPropChanged = true;
  }
  for (int32 i = 0; i < ActorPropLength; ++i)
  {
  {Code_SetDeltaStateArrayInner}
  }
  if(bPropChanged)
  {
	b{Declare_PropertyName}Changed = true;	//	ArrPropDeco_SetDeltaStateByMemOffsetTemp
	{Declare_DeltaStateName}->set_update_{Definition_ProtoName}(true);
    if({Declare_FullStateName} != nullptr && {Declare_FullStateName}->{Definition_ProtoName}_size() > 0)
    {
      const_cast<{Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}*>({Declare_FullStateName})->clear_{Definition_ProtoName}();
    }
  }
  else if(!ForceMarge)
  {
    {Declare_DeltaStateName}->clear_{Definition_ProtoName}();
  }
}
bStateChanged |= b{Declare_PropertyName}Changed;
)EOF";

const static TCHAR* ArrPropDeco_OnChangeStateTemp =
	LR"EOF(
bool  b{Declare_PropertyName}Changed = false; 
if ({Code_HasProtoFieldValue})
{
{Code_SetPropertyValue}
b{Declare_PropertyName}Changed = true;	//	ArrPropDeco_OnChangeStateTemp
}
else if({Code_GetProtoUpdateValue}) 
{
b{Declare_PropertyName}Changed = {Declare_PropPtrName}->Num() != {Code_GetProtoFieldValueFrom}.size();	//	ArrPropDeco_OnChangeStateTemp
{Declare_PropPtrName}->Empty();
}
bStateChanged |= b{Declare_PropertyName}Changed;
)EOF";

const static TCHAR* ArrPropDeco_SetPropertyValueTemp =
	LR"EOF(
const int32 ActorPropLength = {Declare_PropPtrName}->Num();
auto & MessageArr = {Code_GetProtoFieldValueFrom};
const int32 NewStateValueLength = MessageArr.size();
if (ActorPropLength != NewStateValueLength)
{ 
  b{Declare_PropertyName}Changed = true; //	ArrPropDeco_SetPropertyValueTemp
}
{Declare_PropPtrName}->SetNum(NewStateValueLength);
for (int32 i = 0; i < NewStateValueLength; ++i)
{
{Code_SetPropertyValueArrayInner}
}
)EOF";

const static TCHAR* ArrPropDeco_SetPropertyValueByMemOffsetTemp =
	LR"EOF(
bool b{Declare_PropertyName}Changed = false;
{
  bool bPropChanged = false;
  {Code_AssignPropPointers};
  const int32 ActorPropLength = {Declare_PropPtrName}->Num();
  auto & MessageArr = {Code_GetProtoFieldValueFrom};
  const int32 NewStateValueLength = MessageArr.size();
  if (ActorPropLength != NewStateValueLength)
  { 
    bPropChanged = true;
  }
  {Declare_PropPtrName}->SetNum(NewStateValueLength);
  for (int32 i = 0; i < NewStateValueLength; ++i)
  {
  {Code_SetPropertyValueArrayInner}
  }
  if (bPropChanged)
  {
    b{Declare_PropertyName}Changed = true;	//	ArrPropDeco_SetPropertyValueByMemOffsetTemp
  }
}
bStateChanged |= b{Declare_PropertyName}Changed;
)EOF";


class FArrayPropertyDecorator : public FPropertyDecorator
{
public:
	FArrayPropertyDecorator(FProperty* InProperty, IPropertyDecoratorOwner* InOwner);

	virtual ~FArrayPropertyDecorator() override = default;
	
	virtual void PostInit() override;

	virtual FString GetPropertyName() override;

	virtual FString GetCPPType() override;
	
	virtual FString GetPropertyType() override;
	
	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState) override;
	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState, bool ForceFromPointer) override;
	
	virtual FString GetCode_SetDeltaState(const FString& TargetInstance, const FString& FullStateName, const FString& DeltaStateName,  bool ConditionFullStateIsNull) override;
	virtual FString GetCode_SetDeltaStateByMemOffset(const FString& ContainerName, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull) override;

	virtual FString GetCode_HasProtoFieldValueIn(const FString& StateName) override;

	virtual FString GetCode_OnStateChange(const FString& TargetInstanceName, const FString& NewStateName, const FString& AfterSetValueCode) override;
	virtual FString GetCode_SetPropertyValueTo(const FString& TargetInstance, const FString& NewStateName, const FString& AfterSetValueCode) override;
	virtual FString GetCode_OnStateChangeByMemOffset(const FString& ContainerName, const FString& NewStateName) override;

	virtual TArray<FString> GetAdditionalIncludes() override;

	virtual FString GetCode_GetWorldRef() override;

	virtual TArray<TSharedPtr<FStructPropertyDecorator>> GetStructPropertyDecorators() override;

	virtual FString GetProtoFieldsDefinition(int* NextIndex) override;
protected:
	TSharedPtr<FPropertyDecorator> InnerProperty;
};
