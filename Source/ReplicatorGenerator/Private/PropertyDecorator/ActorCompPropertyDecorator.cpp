#include "PropertyDecorator/ActorCompPropertyDecorator.h"

FString FActorCompPropertyDecorator::GetCPPType()
{
	return TEXT("UActorComponent*");
}

FString FActorCompPropertyDecorator::GetProtoStateMessageType()
{
	return TEXT("ActorComponentRef");
}

FString FActorCompPropertyDecorator::GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState)
{
	return FString::Printf(
		TEXT("%s == ChanneldUtils::GetActorComponentByRef(&%s->%s(), %s)"),
		*GetCode_GetPropertyValueFrom(FromActor),
		*FromState, *GetProtoFieldName(), *Owner->GetCode_GetWorldRef()
	);
}

FString FActorCompPropertyDecorator::GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState, bool ForceFromPointer)
{
	return FString::Printf(
		TEXT("%s == ChanneldUtils::GetActorComponentByRef(&%s->%s(), %s)"),
		*GetCode_GetPropertyValueFrom(FromActor, ForceFromPointer),
		*FromState, *GetProtoFieldName(), *Owner->GetCode_GetWorldRef()
	);
}

FString FActorCompPropertyDecorator::GetCode_GetProtoFieldValueFrom(const FString& StateName)
{
	return FString::Printf(
		TEXT("ChanneldUtils::GetActorComponentByRef(&%s->%s(), %s)"),
		*StateName, *GetProtoFieldName(), *Owner->GetCode_GetWorldRef()
	);
}

FString FActorCompPropertyDecorator::GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode)
{
	return FString::Printf(TEXT("%s->mutable_%s()->CopyFrom(ChanneldUtils::GetRefOfActorComponent(%s))"), *StateName, *GetProtoFieldName(), *GetValueCode);
}

FString FActorCompPropertyDecorator::GetCode_SetPropertyValueTo(const FString& TargetInstance, const FString& NewStateName, const FString& AfterSetValueCode)
{
	return FString::Printf(
		TEXT("%s = ChanneldUtils::GetActorComponentByRef(&%s->%s(), %s);\n  bStateChanged = true;\n%s"),
		*GetCode_GetPropertyValueFrom(TargetInstance),
		*NewStateName, *GetProtoFieldName(), *Owner->GetCode_GetWorldRef(),
		*AfterSetValueCode
	);
}

FString FActorCompPropertyDecorator::GetCode_SetDeltaStateArrayInner(const FString& PropertyPointer, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	FormatArgs.Add(TEXT("Declare_DeltaStateName"), DeltaStateName);
	FormatArgs.Add(TEXT("Declare_FullStateName"), FullStateName);
	FormatArgs.Add(TEXT("Definition_ProtoName"), GetProtoFieldName());
	FormatArgs.Add(TEXT("Code_GetWorldRef"), Owner->GetCode_GetWorldRef());
	return FString::Format(ActorCompPropDeco_SetDeltaStateArrayInnerTemp, FormatArgs);
}

FString FActorCompPropertyDecorator::GetCode_SetPropertyValueArrayInner(const FString& PropertyPointer, const FString& NewStateName)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	FormatArgs.Add(TEXT("Code_GetWorldRef"), Owner->GetCode_GetWorldRef());
	return FString::Format(ActorCompPropDeco_OnChangeStateArrayInnerTemp, FormatArgs);
}
