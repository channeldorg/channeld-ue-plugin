#pragma once

#include "IPropertyDecoratorOwner.h"


static const TCHAR* PropDecorator_AssignPropPtrTemp =
	LR"EOF({Ref_AssignTo} = ({Declare_PropertyCPPType}*)((uint8*){Ref_ContainerAddr} + {Num_PropMemOffset}))EOF";

const static TCHAR* PropDecorator_SetDeltaStateTemplate =
	LR"EOF(
bool b{Declare_PropertyName}Changed = false;
if ({Code_BeforeCondition}!({Code_ActorPropEqualToProtoState}))
{
  {Code_SetProtoFieldValue};
  b{Declare_PropertyName}Changed = true;	//	PropDecorator_SetDeltaStateTemplate
}
bStateChanged |= b{Declare_PropertyName}Changed;

)EOF";

const static TCHAR* PropDeco_SetDeltaStateByMemOffsetTemp =
	LR"EOF(
bool b{Declare_PropertyName}Changed  = false;
{
  {Code_AssignPropPointers};
  if(ForceMarge)
  {
    {Code_SetProtoFieldValue};
  }
  if ({Code_BeforeCondition}*{Declare_PropertyPtr} != {Code_GetProtoFieldValue})
  {
    if(!ForceMarge)
    {
      {Code_SetProtoFieldValue};
	  b{Declare_PropertyName}Changed = true;	//	PropDeco_SetDeltaStateByMemOffsetTemp
    }
  }
  bStateChanged |= b{Declare_PropertyName}Changed;
}
)EOF";

const static TCHAR* PropDeco_SetDeltaStateArrayInnerTemp =
	LR"EOF(
{Declare_DeltaStateName}->add_{Definition_ProtoName}((*{Declare_PropertyPtr})[i]);
if (!bPropChanged)
{
  bPropChanged = !((*{Declare_PropertyPtr})[i] == {Declare_FullStateName}->{Definition_ProtoName}()[i]);
}
)EOF";

const static TCHAR* PropDecorator_CallRepNotifyTemplate =
	LR"EOF(
if (b{Declare_PropertyName}Changed)
{
	{Declare_TargetInstance}->ProcessEvent({Declare_TargetInstance}->GetClass()->FindFunctionByName(FName(TEXT("{Declare_FunctionName}"))), {Code_OnRepParams});
}
)EOF";


const static TCHAR* PropDecorator_OnChangeStateTemplate =
	LR"EOF(
//	Testing oi PropDecorator_OnChangeStateTemplate
bool b{Declare_PropertyName}Changed = false;
if ({Code_HasProtoFieldValue} && !({Code_ActorPropEqualToProtoState}))
{
  {Code_SetPropertyValue}
  b{Declare_PropertyName}Changed = true;	//	PropDecorator_OnChangeStateTemplate
}
bStateChanged |= b{Declare_PropertyName}Changed;
)EOF";

const static TCHAR* PropDeco_OnChangeStateByMemOffsetTemp =
	LR"EOF(
bool b{Declare_PropertyName}Changed = false;
{
	//	Testing oi PropDeco_OnChangeStateByMemOffsetTemp
  {Code_AssignPropPointers};
  if ({Code_HasProtoFieldValue} && *{Declare_PropertyPtr} != {Code_GetProtoFieldValue})
  {
    *{Declare_PropertyPtr} = {Code_GetProtoFieldValue};
    b{Declare_PropertyName}Changed = true;	//	PropDeco_OnChangeStateByMemOffsetTemp
  }
  bStateChanged |= b{Declare_PropertyName}Changed;
}
)EOF";

const static TCHAR* PropDeco_OnChangeStateArrayInnerTemp =
	LR"EOF(
if ((*{Declare_PropertyPtr})[i] != MessageArr[i])
{
  (*{Declare_PropertyPtr})[i] = MessageArr[i];
  if (!b{Declare_PropertyName}Changed)
  {
    b{Declare_PropertyName}Changed = true;	//	PropDeco_OnChangeStateArrayInnerTemp
  }
  bStateChanged |= b{Declare_PropertyName}Changed;
}
)EOF";

class FReplicatedActorDecorator;

class FPropertyDecorator : public IPropertyDecoratorOwner
{
public:
	FPropertyDecorator(FProperty* InProperty, IPropertyDecoratorOwner* InOwner);

	virtual ~FPropertyDecorator() = default;

	virtual bool Init(const TFunction<FString()>& SetNameForIllegalPropName);

	virtual void PostInit();

	virtual bool IsBlueprintType() override;

	/**
	 * If the access of the property is private or protect return false, else return true
	 */
	virtual bool IsExternallyAccessible();

	/**
	 * If the property is externally accessible and the owner is not a blueprint generated class return true, else return false
	 */
	virtual bool IsDirectlyAccessible();

	bool IsForceNotDirectlyAccessible() const;
	void SetForceNotDirectlyAccessible(bool bForceNotDirectlyAccessible);
	
	virtual bool HasAnyPropertyFlags(EPropertyFlags PropertyFlags);

	/**
  	  * Get the name of field
  	  */
	virtual FString GetPropertyName();

	/**
 	 * Get the class name of OriginalProperty
 	 */
	virtual FString GetPropertyType() = 0;

	/**
     * Get pointer name
     */
	virtual FString GetPointerName();

	/**
 	 * Get the cpp type by OriginalProperty->GetCPPType()
 	 */
	virtual FString GetCPPType();

	/**
	 * Get the cpp type which is used as the declaration type in the cpp code,
	 * Default is the same as GetCPPType().
	 * When getting the DeclarationCPPType of the struct defined by the blueprint,
	 * the function will return the StructPropertyDecorator custom type name
	 */
	virtual FString GetCompilableCPPType();

	virtual int32 GetMemOffset();

	virtual int32 GetPropertySize();

	virtual int32 GetPropertyMinAlignment();

	virtual FString GetProtoPackageName() override;
	virtual FString GetProtoNamespace() override;
	virtual FString GetProtoStateMessageType() override;

	//	NextIndex will updated
	virtual FString GetProtoFieldsDefinition(int* NextIndex);

	/**
	 * Get the protobuf field rule
	 */
	virtual FString GetProtoFieldRule()
	{
		return ProtoFieldRule;
	}

	/**
	 * Get the protobuf field name
	 */
	virtual FString GetProtoFieldName();

	/**
	 * Get the protobuf field type
	 */
	virtual FString GetProtoFieldType()
	{
		return ProtoFieldType;
	}

	virtual UFunction* FindFunctionByName(const FName& FuncName) override;

	/**
	 * Code that getting property value from outer actor
	 *
	 * For example:
	 *   Actor->bIsCrouched
	 */
	virtual FString GetCode_GetPropertyValueFrom(const FString& TargetInstance);
	virtual FString GetCode_GetPropertyValueFrom(const FString& TargetInstance, bool ForceFromPointer);


	/**
     * Code that getting property value from outer actor
     *
     * For example:
     *   Actor->bIsCrouched = xxx
     */
	virtual FString GetCode_SetPropertyValueTo(const FString& TargetInstance, const FString& NewStateName, const FString& AfterSetValueCode);

	/**
      * Declaration of property pointer
      *
      * For example:
      *   bool* bIsCrouched
      */
	virtual FString GetDeclaration_PropertyPtr();

	virtual FString GetCode_AssignPropPointer(const FString& Container, const FString& AssignTo);
	virtual FString GetCode_AssignPropPointer(const FString& Container, const FString& AssignTo, int32 MemOffset);

	/**
	 * Code that get field value from protobuf message
	 *
	 * For example:
	 *   FullState->biscrouched()
	 */
	virtual FString GetCode_GetProtoFieldValueFrom(const FString& StateName);

	/**
	 * Code that set new value to protobuf message field
	 *
	 * For example:
	 *   DeltaState->set_biscrouched(Character->bIsCrouched)
	 */
	virtual FString GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode);

	/**
	 * Code that protobuf message has field value
	 * For example:
	 *   NewState->has_biscrouched()
	 */
	virtual FString GetCode_HasProtoFieldValueIn(const FString& StateName);

	/**
	 * Code of actor property equal to protobuf state
	 *
	 * For example:
	 *   *ByteProperty01Ptr != FullState->byteproperty01()
	 */
	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState);
	virtual FString GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState, bool ForceFromPointer);

	/**
	 * Code that set delta state
	 * For example:
	 *   if (FullState->biscrouched() != Character->bIsCrouched)
	 *   {
	 *     DeltaState->set_biscrouched(Character->bIsCrouched);
	 *     bStateChanged = true;
	 *   }
	 */
	virtual FString GetCode_SetDeltaState(const FString& TargetInstance, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull = false);

	virtual FString GetCode_SetDeltaStateByMemOffset(const FString& ContainerName, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull = false);

	virtual FString GetCode_SetDeltaStateArrayInner(const FString& PropertyPointer, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull = false);

	/**
	 * The property has RepNotify function and the RepNotify function has parameter
	 */
	virtual bool HasOnRepNotifyParam();

	virtual FString GetCode_CallRepNotify(const FString& TargetInstanceName, const FString& OldValuePointer);

	/**
	 * Code that handle state changes
	 * For example:
	 * 	 if (NewState->has_biscrouched() && NewState->biscrouched() != Character->bIsCrouched)
	 *   {
	 *     Character->bIsCrouched = NewState->biscrouched();
	 *   }
	 */
	virtual FString GetCode_OnStateChange(const FString& TargetInstanceName, const FString& NewStateName, const FString& AfterSetValueCode = TEXT(""));

	virtual FString GetCode_OnStateChangeByMemOffset(const FString& ContainerName, const FString& NewStateName);

	virtual FString GetCode_SetPropertyValueArrayInner(const FString& ArrayPropertyName, const FString& PropertyPointer, const FString& NewStateName);

	virtual TArray<FString> GetAdditionalIncludes() override;

	virtual FString GetCode_GetWorldRef() override;

	virtual bool IsStruct() override;

	virtual TArray<TSharedPtr<FStructPropertyDecorator>> GetStructPropertyDecorators() override;

protected:
	bool bInitialized = false;
	
	IPropertyDecoratorOwner* Owner = nullptr;

	FProperty* OriginalProperty = nullptr;

	FString CompilablePropName;

	// protobuf field rule
	FString ProtoFieldRule = TEXT("optional");

	// protobuf field type
	FString ProtoFieldType;

	bool bForceNotDirectlyAccessible = false;
};
