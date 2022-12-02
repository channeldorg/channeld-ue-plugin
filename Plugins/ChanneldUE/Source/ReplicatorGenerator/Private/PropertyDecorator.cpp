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
	return OriginalProperty->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPublic);
}

bool FPropertyDecorator::IsDirectlyAccessible()
{
	return IsExternallyAccessible() && !Owner->IsBlueprintGenerated();
}

FString FPropertyDecorator::GetPropertyName()
{
	return OriginalProperty->GetName();
}

FString FPropertyDecorator::GetPropertyType()
{
	return TEXT("");
}

FString FPropertyDecorator::GetPointerName()
{
	return GetPropertyName() + TEXT("Ptr");
}

FString FPropertyDecorator::GetCPPType()
{
	return OriginalProperty->GetCPPType();
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

FString FPropertyDecorator::GetCode_GetPropertyValueFrom(const FString& TargetInstance)
{
	if(IsDirectlyAccessible())
	{
		return FString::Printf(TEXT("%s->%s"), *TargetInstance, *GetPropertyName());
	}
	else
	{
		return TEXT("*") + GetPointerName();
	}
}

FString FPropertyDecorator::GetCode_SetPropertyValueTo(const FString& TargetInstance, const FString& GetValueCode)
{
	return TEXT("");
}

FString FPropertyDecorator::GetCode_GetProtoFieldValueFrom(const FString& StateName)
{
	return FString::Printf(TEXT("%s->%s()"), *StateName, *GetProtoFieldName());
}

FString FPropertyDecorator::GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode)
{
	return FString::Printf(TEXT("%s->set_%s(%s)"), *StateName, *GetProtoFieldName(), *GetValueCode);
}

FString FPropertyDecorator::GetCode_HasProtoFieldValueIn(const FString& StateName)
{
	return FString::Printf(TEXT("%s->has_%s()"), *StateName, *GetProtoFieldName());
}

FString FPropertyDecorator::GetCode_SetDeltaState(const FString& TargetInstance, const FString& FullStateName, const FString& DeltaStateName)
{
	return TEXT("");
}

FString FPropertyDecorator::GetCode_OnStateChange(const FString& TargetInstance, const FString& NewStateName)
{
	return TEXT("");
}