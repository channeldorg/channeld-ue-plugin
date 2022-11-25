#pragma once

class FReplicatedActorDecorator;

class FPropertyDecorator
{
public:
	FPropertyDecorator() = default;

	virtual ~FPropertyDecorator() = default;

	virtual bool Init(FProperty* Property, FReplicatedActorDecorator* InOwner);

	virtual bool IsBlueprintType();

	virtual bool IsExternallyAccessible();

	/**
	 * Get the name of property declare in outer actor
	 */
	virtual FString GetPropertyName();

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
	 * For example:
	 *   optional bool bIsCrouched
	 */
	virtual FString GetDefinition_ProtoField();

	/**
	 * Get the protobuf field definition with field number
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
	virtual FString GetCode_GetPropertyValueFrom(const FString& TargetInstanceRef);

	/**
     * Code that getting property value from outer actor
     *
     * For example:
     *   Actor->bIsCrouched
     */
	virtual FString GetCode_SetPropertyValueTo(const FString& TargetInstanceRef, const FString& InValue);

	/**
	 * Code that get field value from protobuf message
	 *
	 * For example:
	 *   FullState->biscrouched()
	 */
	virtual FString GetCode_GetProtoFieldValueFrom(const FString& MessageRef);

	/**
	 * Code that set new value to protobuf message field
	 *
	 * For example:
	 *   DeltaState->set_biscrouched(Character->bIsCrouched)
	 */
	virtual FString GetCode_SetProtoFieldValueTo(const FString& MessageRef, const FString& InValue);

	/**
	 * Code that protobuf message has field value
	 * For example:
	 *   NewState->has_biscrouched()
	 */
	virtual FString GetCode_HasProtoFieldValueIn(const FString& MessageRef);

	/**
	 * Code that set delta state
	 * For example:
	 *   if (FullState->biscrouched() != Character->bIsCrouched)
	 *   {
	 *     DeltaState->set_biscrouched(Character->bIsCrouched);
	 *     bStateChanged = true;
	 *   }
	 */
	virtual FString GetCode_SetDeltaState(const FString& TargetInstanceRef, const FString& FullStateRef, const FString& DeltaStateRef);

	/**
	 * Code that handle state changes
	 * For example:
	 * 	 if (NewState->has_biscrouched() && NewState->biscrouched() != Character->bIsCrouched)
	 *   {
	 *     Character->bIsCrouched = NewState->biscrouched();
	 *   }
	 */
	virtual FString GetCode_OnStateChange(const FString& TargetInstanceRef, const FString& NewStateRef);

protected:
	FReplicatedActorDecorator* Owner = nullptr;

	FProperty* OriginalProperty = nullptr;

	// protobuf field rule
	FString ProtoFieldRule;

	// protobuf field type
	FString ProtoFieldType;
};
