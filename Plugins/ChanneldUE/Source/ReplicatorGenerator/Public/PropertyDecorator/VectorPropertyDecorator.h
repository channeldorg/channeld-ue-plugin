#pragma once
#include "PropertyDecorator.h"

const static TCHAR* VectorPropDeco_SetDeltaStateByMemOffsetTemp =
	LR"EOF(
{
  {Code_AssignPropPointers};
  if(ForceMarge)
  {
    {Code_SetProtoFieldValue};
  }
  if ({Code_BeforeCondition}ChanneldUtils::CheckDifference(*{Declare_PropertyPtr}, &{Code_GetProtoFieldValue}))
  {
    if(!ForceMarge)
    {
      {Code_SetProtoFieldValue};
    }
    bStateChanged = true;
  }
}
)EOF";

const static TCHAR* VectorPropDeco_SetDeltaStateArrayInnerTemp =
	LR"EOF(
{Definition_PropertyType}& PropItem = (*{Declare_PropertyPtr})[i];
unrealpb::{Definition_ProtoStateMessageType}* NewOne = {Declare_DeltaStateName}->add_{Definition_ProtoName}();
ChanneldUtils::{FunctionName_SetXXXToPB}(NewOne, PropItem);
if (!bPropChanged)
{
  bPropChanged = ChanneldUtils::CheckDifference(PropItem, &{Declare_DeltaStateName}->{Definition_ProtoName}(i));
}
)EOF";

const static TCHAR* VectorPropDeco_OnChangeStateByMemOffsetTemp =
	LR"EOF(
{
  {Code_AssignPropPointers};
  if ({Code_HasProtoFieldValue} && ChanneldUtils::CheckDifference(*{Declare_PropertyPtr}, &{Code_GetProtoFieldValue}))
  {
    ChanneldUtils::{FunctionName_SetXXXFromPB}(*{Declare_PropertyPtr}, {Code_GetProtoFieldValue});
    bStateChanged = true;
  }
}
)EOF";

const static TCHAR* VectorPropDeco_OnChangeStateArrayInnerTemp =
	LR"EOF(
if (ChanneldUtils::CheckDifference((*{Declare_PropertyPtr})[i], &MessageArr[i]))
{
  ChanneldUtils::{FunctionName_SetXXXFromPB}((*{Declare_PropertyPtr})[i], MessageArr[i]);
  if (!bPropChanged)
  {
    bPropChanged = true;
  }
}
)EOF";

class FVectorPropertyDecorator : public FPropertyDecorator
{
public:
	FVectorPropertyDecorator(FProperty* InProperty, IPropertyDecoratorOwner* InOwner)
		: FPropertyDecorator(InProperty, InOwner)
	{
	}

	virtual ~FVectorPropertyDecorator() override = default;
	
	virtual FString GetFunctionName_SetXXXFromPB() const;
	virtual FString GetFunctionName_SetXXXToPB() const;
	
	virtual FString GetPropertyType() override;

	virtual FString GetProtoFieldType() override;

	virtual FString GetProtoPackageName() override;
	virtual FString GetProtoNamespace() override;
	virtual FString GetProtoStateMessageType() override;
	
	virtual FString GetCode_SetPropertyValueTo(const FString& TargetInstance, const FString& NewStateName, const FString& AfterSetValueCode) override;

	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState) override;
	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState, bool ForceFromPointer) override;

	virtual FString GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode) override;

	virtual FString GetCode_SetDeltaStateByMemOffset(const FString& ContainerName, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull = false) override;
	virtual FString GetCode_SetDeltaStateArrayInner(const FString& PropertyPointer, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull) override;

	virtual FString GetCode_OnStateChangeByMemOffset(const FString& ContainerName, const FString& NewStateName) override;

	virtual FString GetCode_SetPropertyValueArrayInner(const FString& PropertyPointer, const FString& NewStateName) override;

	virtual TArray<FString> GetAdditionalIncludes() override;
};
