#pragma once

#include "IPropertyDecoratorOwner.h"

static const TCHAR* PropDecorator_GetCode_AssignPropertyPointerTemplate =
	LR"EOF({Ref_AssignTo} = {Ref_ContainerTemplate}->FindPropertyByName(FName(TEXT("{Declare_PropertyName}")))->ContainerPtrToValuePtr<{Declare_PropertyCPPType}>({Ref_ContainerAddr}))EOF";

const static TCHAR* PropertyDecorator_SetDeltaStateTemplate =
	LR"EOF(
if (!({Code_ActorPropEqualToProtoState}))
{
  {Code_SetProtoFieldValue};
  bStateChanged = true;
}
)EOF";

const static TCHAR* PropertyDecorator_OnChangeStateTemplate =
	LR"EOF(
if ({Code_HasProtoFieldValue} && !({Code_ActorPropEqualToProtoState}))
{
  {Code_SetPropertyValue}
}
)EOF";

class FReplicatedActorDecorator;

class FPropertyDecorator : public IPropertyDecoratorOwner
{
public:
	FPropertyDecorator(FProperty* InProperty, IPropertyDecoratorOwner* InOwner): Owner(InOwner), OriginalProperty(InProperty)
	{
	}

	virtual ~FPropertyDecorator() = default;

	virtual bool Init();
	
	virtual bool IsBlueprintType() override;

	/**
	 * If the access of the property is private or protect return false, else return true
	 */
	virtual bool IsExternallyAccessible();

	/**
	 * If the property is externally accessible and the owner is not a blueprint generated class return true, else return false
	 */
	virtual bool IsDirectlyAccessible();

	virtual FString GetPropertyName();

	/**
 	 * Get the class name of OriginalProperty
 	 */
	virtual FString GetPropertyType();

	/**
     * Get pointer name
     */
	virtual FString GetPointerName();

	/**
 	 * Get cpp type
 	 */
	virtual FString GetCPPType();
	
	virtual FString GetProtoPackageName() override;
	virtual FString GetProtoNamespace() override;
	virtual FString GetProtoStateMessageType() override;
	
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

	/**
	 * Get the protobuf field definition
	 * 
	 * For example:
	 *   optional bool bIsCrouched
	 */
	virtual FString GetDefinition_ProtoField();

	/**
	 * Get the protobuf field definition with field number
	 * 
	 * For example:
	 *   optional bool bIsCrouched = 6
	 */
	virtual FString GetDefinition_ProtoField(int32 FieldNumber);

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
	virtual FString GetCode_SetPropertyValueTo(const FString& TargetInstance, const FString& NewStataName, const FString& AfterSetValueCode);

	/**
      * Declaration of property pointer
      *
      * For example:
      *   bool* bIsCrouched
      */
	virtual FString GetDeclaration_PropertyPtr();
	
	/**
	 * Code of assign property pointer
	 *
	 * For example:
	 *     CastFieldChecked<const FByteProperty>(Actor->GetClass()->FindPropertyByName(FName("ByteProperty01")))->Property->ContainerPtrToValuePtr<uint8>(Actor.Get())
	 */
	virtual FString GetCode_AssignPropertyPointer(const FString& Container, const FString& ContainerTemplate, const FString& AssignTo);

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
	virtual FString GetCode_SetDeltaState(const FString& TargetInstance, const FString& FullStateName, const FString& DeltaStateName);

	/**
	 * Code that handle state changes
	 * For example:
	 * 	 if (NewState->has_biscrouched() && NewState->biscrouched() != Character->bIsCrouched)
	 *   {
	 *     Character->bIsCrouched = NewState->biscrouched();
	 *   }
	 */
	virtual FString GetCode_OnStateChange(const FString& TargetInstance, const FString& NewStateName);

	/*
	 * Get addition include header file
	 */
	virtual TArray<FString> GetAdditionalIncludes();

protected:
	IPropertyDecoratorOwner* Owner = nullptr;

	FProperty* OriginalProperty = nullptr;

	// protobuf field rule
	FString ProtoFieldRule = TEXT("optional");

	// protobuf field type
	FString ProtoFieldType;
};
