#include "PropertyDecorator.h"
#include "ReplicatedActorDecorator.h"

bool FPropertyDecorator::Init(FProperty* Property, FReplicatedActorDecorator* InOwner)
{
	OriginalProperty = Property;
	Owner = InOwner;
	return true;
}

bool FPropertyDecorator::IsBlueprintType()
{
	return false;
}

bool FPropertyDecorator::IsExternallyAccessible()
{
	return false;
}

FString FPropertyDecorator::GetPropertyName()
{
	return OriginalProperty->GetName();
}

FString FPropertyDecorator::GetProtoFieldName()
{
	return GetPropertyName().ToLower();
}

FString FPropertyDecorator::GetDefinition_ProtoField()
{
	const FString& FieldRule = GetProtoFieldRule();
	const FString SpaceChar = TEXT(" ");
	return FString(FieldRule.IsEmpty() ? TEXT("") : FieldRule + SpaceChar) + GetProtoFieldType() + SpaceChar + GetProtoFieldName();
}

FString FPropertyDecorator::GetDefinition_ProtoField(int32 FieldNumber)
{
	return GetDefinition_ProtoField() + TEXT(" = ") + FString::FromInt(FieldNumber);
}

FString FPropertyDecorator::GetCode_GetPropertyValueFrom(const FString& TargetInstanceRef)
{
	return TEXT("");
}

FString FPropertyDecorator::GetCode_SetPropertyValueTo(const FString& TargetInstanceRef, const FString& InValue)
{
	return TEXT("");
}

FString FPropertyDecorator::GetCode_GetProtoFieldValueFrom(const FString& MessageRef)
{
	return TEXT("");
}

FString FPropertyDecorator::GetCode_SetProtoFieldValueTo(const FString& MessageRef, const FString& InValue)
{
	return TEXT("");
}

FString FPropertyDecorator::GetCode_HasProtoFieldValueIn(const FString& MessageRef)
{
	return TEXT("");
}

FString FPropertyDecorator::GetCode_SetDeltaState(const FString& TargetInstanceRef, const FString& FullStateRef, const FString& DeltaStateRef)
{
	return TEXT("");
}

FString FPropertyDecorator::GetCode_OnStateChange(const FString& TargetInstanceRef, const FString& NewStateRef)
{
	return TEXT("");
}