#pragma once
#include "PropertyDecorator.h"

// const static TCHAR* StructPropertyDeco_SetDeltaStateTemplate =
// 	LR"EOF(
// if ({Code_GetPropertyValue} != {Code_GetProtoFieldValue})
// {
//   {Code_SetProtoFieldValue};
//   bStateChanged = true;
// }
// )EOF";
//
// const static TCHAR* StructPropDeco_OnChangeStateTemplate =
// 	LR"EOF(
// if ({Code_HasProtoFieldValue} && {Code_GetPropertyValue} != {Code_GetProtoFieldValue})
// {
//   {Code_SetPropertyValue};
// }
// )EOF";

const static TCHAR* StructPropDeco_PropPtrGroupStructTemp =
	LR"EOF(
struct {Declare_PropPtrGroupStructName}
{
  {Declare_PropPtrGroupStructName}() {}
  {Declare_PropPtrGroupStructName}(void* Container)
  {
{Code_AssignPropPointers}
  }

{Declare_PropertyPointers}
  
  bool Merge(const {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}* FullState, {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}* DeltaState, UWorld* World)
  {
    bool bIsFullStateNull = FullState == nullptr;
    bool bStateChanged = false;
{Code_SetDeltaStates}
    return bStateChanged;
  }
  
  static bool Merge(void* Container, const {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}* FullState, {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}* DeltaState, UWorld* World, bool ForceMarge)
  {
    bool bIsFullStateNull = FullState == nullptr;
    bool bStateChanged = false;
{Code_StaticSetDeltaStates}
    return bStateChanged;
  }
  
  bool SetPropertyValue(const {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}* NewState, UWorld* World)
  {
    bool bStateChanged = false;
{Code_OnStateChange}
    return bStateChanged;
  }
  
  static bool SetPropertyValue(void* Container, const {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}* NewState, UWorld* World)
  {
    bool bStateChanged = false;
{Code_StaticOnStateChange}
    return bStateChanged;
  }

};

struct {Declare_PropCompilableStructName}
{
	{Code_StructCopyProperties}
};
)EOF";

const static TCHAR* StructPropDeco_AssignPropPtrTemp =
	LR"EOF(
void* PropertyAddr = (uint8*){Ref_ContainerAddr} + {Num_PropMemOffset};
{Ref_AssignTo} = {Declare_PropPtrGroupStructName}(PropertyAddr))EOF";

const static TCHAR* StructPropDeco_AssignPropPtrDispersedlyTemp =
	LR"EOF(
FStructProperty* StructProperty = CastFieldChecked<FStructProperty>({Ref_ContainerTemplate}->FindPropertyByName(TEXT("{Declare_PropertyName}")));
void* PropertyAddr = StructProperty->ContainerPtrToValuePtr<void>({Ref_ContainerAddr});
{Ref_AssignTo} = {Declare_PropPtrGroupStructName}(StructProperty->Struct, PropertyAddr))EOF";

const static TCHAR* StructPropDeco_AssignPropPtrOrderlyTemp =
	LR"EOF(
FStructProperty* StructProperty = CastFieldChecked<FStructProperty>({Ref_ContainerTemplate});
void* PropertyAddr = StructProperty->ContainerPtrToValuePtr<void>({Ref_ContainerAddr});
{Ref_AssignTo} = {Declare_PropPtrGroupStructName}(StructProperty->Struct, PropertyAddr))EOF";

const static TCHAR* StructPropDeco_SetDeltaStateArrayInnerTemp =
	LR"EOF(
{
  auto PropItem = &(*{Declare_PropertyPtr})[i];
  auto FullStateValue = i <  FullStateValueLength ? &{Declare_FullStateName}->{Definition_ProtoName}()[i] : nullptr;
  auto NewOne = {Declare_DeltaStateName}->add_{Definition_ProtoName}();
  bool bItemChanged = {Declare_PropPtrGroupStructName}::Merge(PropItem, FullStateValue, NewOne, {Code_GetWorldRef}, true);
  if (!bPropChanged && bItemChanged)
  {
  	bPropChanged = true;
  }
}
)EOF";

const static TCHAR* StructPropDeco_SetPropertyValueArrayInnerTemp =
	LR"EOF(
if ({Declare_PropPtrGroupStructName}::SetPropertyValue(&(*{Declare_PropertyPtr})[i], &MessageArr[i], {Code_GetWorldRef}) && !bPropChanged)
{
  bPropChanged = true;
}
)EOF";

class FStructPropertyDecorator : public FPropertyDecorator
{
public:

	FStructPropertyDecorator(FProperty* InProperty, IPropertyDecoratorOwner* InOwner);
	
	virtual void PostInit() override;
	
	virtual bool IsExternallyAccessible() override;

	virtual bool IsDeclaredInCPP() override;

	virtual FString GetCompilableCPPType() override;

	virtual FString GetProtoFieldType() override;
	
	virtual FString GetProtoPackageName() override;
	
	virtual FString GetProtoNamespace() override;

	virtual FString GetProtoStateMessageType() override;
	
	virtual FString GetPropertyType() override;
	
	virtual FString GetDeclaration_PropertyPtr() override;
	
	virtual FString GetCode_AssignPropPointer(const FString& Container, const FString& AssignTo) override;

	virtual FString GetCode_AssignPropPtrDispersedly(const FString& Container, const FString& ContainerTemplate, const FString& AssignTo) override;
	
	virtual FString GetCode_AssignPropPtrOrderly(const FString& Container, const FString& ContainerTemplate, const FString& AssignTo) override;
	
	virtual TArray<FString> GetAdditionalIncludes() override;
	
	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState) override;
	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState, bool ForceFromPointer) override;
	
	virtual FString GetCode_SetDeltaState(const FString& TargetInstance, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull = false) override;

	virtual FString GetCode_SetDeltaStateByMemOffset(const FString& ContainerName, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull = false) override;

	virtual FString GetCode_SetDeltaStateArrayInner(const FString& TargetInstance, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull = false) override;

	virtual FString GetCode_SetPropertyValueTo(const FString& TargetInstance, const FString& NewStateName, const FString& AfterSetValueCode) override;

	virtual FString GetCode_OnStateChangeByMemOffset(const FString& ContainerName, const FString& NewStateName) override;

	virtual FString GetCode_SetPropertyValueArrayInner(const FString& TargetInstance, const FString& NewStateName) override;

	FString GetDeclaration_PropPtrGroupStruct();

	FString GetDeclaration_PropPtrGroupStructName();

	FString GetDefinition_ProtoStateMessage();

	virtual FString GetCode_GetWorldRef() override;
	
protected:
	TArray<TSharedPtr<FPropertyDecorator>> Properties;

	bool bIsBlueprintVariable;
};
