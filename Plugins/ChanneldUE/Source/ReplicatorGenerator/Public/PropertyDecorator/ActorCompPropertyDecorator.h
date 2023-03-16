#pragma once
#include "UnrealObjectPropertyDecorator.h"

const static TCHAR* ActorCompPropDeco_SetDeltaStateArrayInnerTemp =
	LR"EOF(
UObject * & PropItem = (*{Declare_PropertyPtr})[i];
unrealpb::ActorComponentRef* NewOne = {Declare_DeltaStateName}->add_{Definition_ProtoName}();
*NewOne = ChanneldUtils::GetRefOfActorComponent(PropItem);
if (!bPropChanged)
{
  bPropChanged = !(PropItem == ChanneldUtils::GetActorComponentByRef(&{Declare_FullStateName}->{Definition_ProtoName}()[i], {Code_GetWorldRef}));
}
)EOF";

const static TCHAR* ActorCompPropDeco_OnChangeStateArrayInnerTemp =
	LR"EOF(
UActorComponent* NewCompRef = ChanneldUtils::GetActorComponentByRef(&MessageArr[i], {Code_GetWorldRef});
if ((*{Declare_PropertyPtr})[i] != NewCompRef)
{
  (*{Declare_PropertyPtr})[i] = NewCompRef;
  if (!bPropChanged)
  {
    bPropChanged = true;
  }
}
)EOF";

class FActorCompPropertyDecorator : public FUnrealObjectPropertyDecorator
{
public:
	FActorCompPropertyDecorator(FProperty* InProperty, IPropertyDecoratorOwner* InOwner)
		: FUnrealObjectPropertyDecorator(InProperty, InOwner)
	{
	}
	virtual FString GetCPPType() override;

	virtual FString GetProtoStateMessageType() override;

	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState) override;
	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState, bool ForceFromPointer) override;

	virtual FString GetCode_GetProtoFieldValueFrom(const FString& StateName) override;

	virtual FString GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode) override;

	virtual FString GetCode_SetPropertyValueTo(const FString& TargetInstance, const FString& NewStateName, const FString& AfterSetValueCode) override;
	virtual FString GetCode_SetDeltaStateArrayInner(const FString& PropertyPointer, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull) override;

	virtual FString GetCode_SetPropertyValueArrayInner(const FString& PropertyPointer, const FString& NewStateName) override;

};
