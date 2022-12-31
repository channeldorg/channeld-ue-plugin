#include "PropertyDecorator/UnrealObjectPropertyDecorator.h"

FString FUnrealObjectPropertyDecorator::GetCPPType()
{
	return TEXT("UObject*");
}

FString FUnrealObjectPropertyDecorator::GetPropertyType()
{
	return TEXT("FObjectProperty");
}

FString FUnrealObjectPropertyDecorator::GetProtoFieldType()
{
	return FString::Printf(TEXT("%s.%s"), *GetProtoPackageName(), *GetProtoStateMessageType());
}

FString FUnrealObjectPropertyDecorator::GetProtoPackageName()
{
	return TEXT("unrealpb");
}

FString FUnrealObjectPropertyDecorator::GetProtoNamespace()
{
	return GetProtoPackageName();
}

FString FUnrealObjectPropertyDecorator::GetProtoStateMessageType()
{
	return TEXT("UnrealObjectRef");
}

FString FUnrealObjectPropertyDecorator::GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState)
{
	return FString::Printf(
		TEXT("%s == ChanneldUtils::GetObjectByRef(&%s->%s(), %s)"),
		*GetCode_GetPropertyValueFrom(FromActor),
		*FromState, *GetProtoFieldName(), *Owner->GetCode_GetWorldRef()
	);
}

FString FUnrealObjectPropertyDecorator::GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState, bool ForceFromPointer)
{
	return FString::Printf(
		TEXT("%s == ChanneldUtils::GetObjectByRef(&%s->%s(), %s)"),
		*GetCode_GetPropertyValueFrom(FromActor, ForceFromPointer),
		*FromState, *GetProtoFieldName(), *Owner->GetCode_GetWorldRef()
	);
}

FString FUnrealObjectPropertyDecorator::GetCode_GetProtoFieldValueFrom(const FString& StateName)
{
	return FString::Printf(
		TEXT("ChanneldUtils::GetObjectByRef(&%s->%s(), %s)"),
		*StateName, *GetProtoFieldName(), *Owner->GetCode_GetWorldRef()
	);
}

FString FUnrealObjectPropertyDecorator::GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode)
{
	return FString::Printf(TEXT("%s->mutable_%s()->CopyFrom(ChanneldUtils::GetRefOfObject(%s))"), *StateName, *GetProtoFieldName(), *GetValueCode);
}

FString FUnrealObjectPropertyDecorator::GetCode_SetPropertyValueTo(const FString& TargetInstance, const FString& NewStateName, const FString& AfterSetValueCode)
{
	return FString::Printf(
		TEXT("%s = ChanneldUtils::GetObjectByRef(&%s->%s(), %s);\n  bStateChanged = true;\n%s"),
		*GetCode_GetPropertyValueFrom(TargetInstance),
		*NewStateName, *GetProtoFieldName(), *Owner->GetCode_GetWorldRef(),
		*AfterSetValueCode
	);
}

FString FUnrealObjectPropertyDecorator::GetCode_SetDeltaStateArrayInner(const FString& PropertyPointer, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	FormatArgs.Add(TEXT("Declare_DeltaStateName"), DeltaStateName);
	FormatArgs.Add(TEXT("Declare_FullStateName"), FullStateName);
	FormatArgs.Add(TEXT("Definition_ProtoName"), GetProtoFieldName());
	FormatArgs.Add(TEXT("Code_GetWorldRef"), Owner->GetCode_GetWorldRef());
	return FString::Format(UObjPropDeco_SetDeltaStateArrayInnerTemp, FormatArgs);
}

FString FUnrealObjectPropertyDecorator::GetCode_SetPropertyValueArrayInner(const FString& PropertyPointer, const FString& NewStateName)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	FormatArgs.Add(TEXT("Code_GetWorldRef"), Owner->GetCode_GetWorldRef());
	return FString::Format(UObjPropDeco_OnChangeStateArrayInnerTemp, FormatArgs);
}

TArray<FString> FUnrealObjectPropertyDecorator::GetAdditionalIncludes()
{
	return TArray<FString>{TEXT("ChanneldUtils.h")};
}

FString FUnrealObjectPropertyDecorator::GetCode_GetWorldRef()
{
	return TEXT("");
}
