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
  {Declare_PropPtrGroupStructName}(UStruct* ContainerTemplate, void* Container)
  {
{Code_AssignPropertyPointers}
  }
{Declare_PropertyPointers}

bool IsSameAs(const {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}* InState)
{
  return {Code_PointerValuesEqualToStateValues};
}

bool Merge(const {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}* FullState, {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}* DeltaState)
{
  bool bStateChanged = false;
{Code_SetDeltaStates}
  return bStateChanged;
}

bool SetPropertyValue(const {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}* NewState)
{
  bool bStateChanged = false;
{Code_OnStateChange}
  return bStateChanged;
}

};
)EOF";

const static TCHAR* StructPropDeco_AssignPropertyPointerTemp =
	LR"EOF(
FStructProperty* StructProperty = CastFieldChecked<FStructProperty>({Ref_ContainerTemplate}->FindPropertyByName(TEXT("{Declare_PropertyName}")));
void* PropertyAddr = StructProperty->ContainerPtrToValuePtr<void>({Ref_ContainerAddr});
{Ref_AssignTo} = {Declare_PropPtrGroupStructName}(StructProperty->Struct, PropertyAddr))EOF";

class FStructPropertyDecorator : public FPropertyDecorator
{
public:

	FStructPropertyDecorator(FProperty* InProperty, IPropertyDecoratorOwner* InOwner);

	virtual bool Init() override;
	
	virtual bool IsBlueprintType() override;
	
	virtual FString GetProtoFieldType() override;
	
	virtual FString GetProtoPackageName() override;
	
	virtual FString GetProtoNamespace() override;

	virtual FString GetProtoStateMessageType() override;
	
	virtual FString GetPropertyType() override;
	
	virtual FString GetDeclaration_PropertyPtr() override;
	
	virtual FString GetCode_AssignPropertyPointer(const FString& Container, const FString& ContainerTemplate, const FString& AssignTo) override;

	virtual TArray<FString> GetAdditionalIncludes() override;
	
	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState) override;
	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState, bool ForceFromPointer) override;
	
	virtual FString GetCode_SetDeltaState(const FString& TargetInstance, const FString& FullStateName, const FString& DeltaStateName) override;
	
	virtual FString GetCode_SetPropertyValueTo(const FString& TargetInstance, const FString& NewStateName, const FString& AfterSetValueCode) override;
	
	FString GetDeclaration_PropPtrGroupStruct();

	FString GetDeclaration_PropPtrGroupStructName();

	FString GetDefinition_ProtoStateMessage();
	
	
protected:
	TArray<TSharedPtr<FPropertyDecorator>> Properties;

	bool bIsBlueprintVariable;
};
