#pragma once
#include "PropertyDecorator.h"

const static TCHAR* StructPropDeco_PropPtrGroupStructTemp =
	LR"EOF(
struct {Declare_PropPtrGroupStructName}
{
  {Declare_PropPtrGroupStructName}() {}
  {Declare_PropPtrGroupStructName}(void* Container)
  {
{Code_AssignPropPointers}
  }

  {Declare_PropPtrGroupStructName}(void* Params, FOutParmRec* OutParams)
  {
{Code_AssignPropPointersForRPC}
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

const static TCHAR* StructPropDeco_AssignPropPtrStatic =
	LR"EOF(
uint8* PropertyAddr = (uint8*){Ref_ContainerAddr} + {Num_PropMemOffset};
{Ref_AssignTo} = {Declare_PropPtrGroupStructName}(PropertyAddr))EOF";

const static TCHAR* StructPropDeco_AssignPropPtrDynamic =
    LR"EOF(
FString PropertyName = TEXT("{Declare_PropertyName}");
uint8* PropertyAddr = (uint8*){Ref_ContainerAddr};
int32* OffsetPtr = PropPointerMemOffsetCache.Find(PropertyName);
if (OffsetPtr != nullptr)
{    
    {Ref_AssignTo} = ({Declare_PropPtrGroupStructName})(PropertyAddr + *OffsetPtr);
}
else
{
    FProperty* Property = ActorClass->FindPropertyByName(FName(*PropertyName));
    if (Property)
    {
        int32 Offset = Property->GetOffset_ForInternal();
        PropPointerMemOffsetCache.Emplace(PropertyName, Offset);
        {Ref_AssignTo} = ({Declare_PropPtrGroupStructName})(PropertyAddr + Offset);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("%s Replicator construct, but could not find property(%s) in cache or by name."), *ActorClass->GetName(), *PropertyName);
        {Ref_AssignTo} = ({Declare_PropPtrGroupStructName})(PropertyAddr + {Num_PropMemOffset});
    }
}
)EOF";

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
if ({Declare_PropPtrGroupStructName}::SetPropertyValue(&(*{Declare_PropertyPtr})[i], &MessageArr[i], {Code_GetWorldRef}) && !b{Declare_PropertyName}Changed)
{
  b{Declare_PropertyName}Changed = true;
}
)EOF";

class FStructPropertyDecorator : public FPropertyDecorator
{
public:

	FStructPropertyDecorator(FProperty* InProperty, IPropertyDecoratorOwner* InOwner);
	
	virtual void PostInit() override;
	
	virtual FString GetCompilableCPPType() override;

	virtual FString GetProtoFieldType() override;
	
	virtual FString GetProtoStateMessageType() override;
	
	virtual FString GetPropertyType() override;
	
	virtual FString GetDeclaration_PropertyPtr() override;

	virtual FString GetCode_AssignPropPointerStatic(const FString& Container, const FString& AssignTo) override;
	virtual FString GetCode_AssignPropPointerDynamic(const FString& Container, const FString& AssignTo) override;
	
	virtual TArray<FString> GetAdditionalIncludes() override;
	
	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState) override;
	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState, bool ForceFromPointer) override;
	
	virtual FString GetCode_SetDeltaState(const FString& TargetInstance, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull = false) override;

	virtual FString GetCode_SetDeltaStateByMemOffset(const FString& ContainerName, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull = false) override;

	virtual FString GetCode_SetDeltaStateArrayInner(const FString& TargetInstance, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull = false) override;

	virtual FString GetCode_SetPropertyValueTo(const FString& TargetInstance, const FString& NewStateName, const FString& AfterSetValueCode) override;

	virtual FString GetCode_OnStateChangeByMemOffset(const FString& ContainerName, const FString& NewStateName) override;

	virtual FString GetCode_SetPropertyValueArrayInner(const FString& ArrayPropertyName, const FString& TargetInstance, const FString& NewStateName) override;

	virtual FString GetDeclaration_PropPtrGroupStruct();

	FString GetDeclaration_PropPtrGroupStructName();

	FString GetDefinition_ProtoStateMessage();

	virtual FString GetCode_GetWorldRef() override;

	virtual bool IsStruct() override;

	virtual TArray<TSharedPtr<FStructPropertyDecorator>> GetStructPropertyDecorators() override;
	
protected:
	TArray<TSharedPtr<FPropertyDecorator>> Properties;

	bool bIsBlueprintVariable;
};
