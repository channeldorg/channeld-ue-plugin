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
  if (ChanneldUtils::CheckDifference(*PropAddr, {Declare_FullStateName}->{Definition_ProtoName}())))
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
FVector& PropItem = (*{Declare_PropertyPtr})[i];
unrealpb::FVector* NewOne = {Declare_DeltaStateName}->add_{Definition_ProtoName}();
ChanneldUtils::SetVectorToPB(NewOne, PropItem);
if (!bPropChanged)
{
  bPropChanged = ChanneldUtils::CheckDifference(PropItem, {Declare_FullStateName}->{Definition_ProtoName}()[i]);
}
)EOF";

const static TCHAR* VectorPropDeco_OnChangeStateArrayInnerTemp =
	LR"EOF(
FVector NewVector;
ChanneldUtils::SetVectorFromPB(NewVector, MessageArr[i]);
if ((*{Declare_PropertyPtr})[i] != NewVector)
{
  (*{Declare_PropertyPtr})[i] = NewVector;
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

	virtual FString GetPropertyType() override;

	virtual FString GetProtoFieldType() override;

	virtual FString GetProtoPackageName() override;
	virtual FString GetProtoNamespace() override;
	virtual FString GetProtoStateMessageType() override;

	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState) override;
	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState, bool ForceFromPointer) override;

	virtual FString GetCode_GetProtoFieldValueFrom(const FString& StateName) override;
	virtual FString GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode) override;

	virtual FString GetCode_SetDeltaStateArrayInner(const FString& PropertyPointer, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull) override;

	virtual FString GetCode_SetPropertyValueArrayInner(const FString& PropertyPointer, const FString& NewStateName) override;

	virtual TArray<FString> GetAdditionalIncludes() override;
};
