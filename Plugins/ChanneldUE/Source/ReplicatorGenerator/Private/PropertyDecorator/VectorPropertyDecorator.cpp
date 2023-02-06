#include "PropertyDecorator/VectorPropertyDecorator.h"


FString FVectorPropertyDecorator::GetPropertyType()
{
	return TEXT("FVector");
}

FString FVectorPropertyDecorator::GetProtoFieldType()
{
	return FString::Printf(TEXT("%s.%s"), *GetProtoPackageName(), *GetProtoStateMessageType());
}

FString FVectorPropertyDecorator::GetProtoPackageName()
{
	return TEXT("unrealpb");
}

FString FVectorPropertyDecorator::GetProtoNamespace()
{
	return GetProtoPackageName();
}

FString FVectorPropertyDecorator::GetProtoStateMessageType()
{
	return TEXT("FVector");
}

FString FVectorPropertyDecorator::GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState)
{
	return FString::Printf(TEXT("!ChanneldUtils::CheckDifference(%s, &%s->%s())"), *GetCode_GetPropertyValueFrom(FromActor), *FromState, *GetProtoFieldName());
}

FString FVectorPropertyDecorator::GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState, bool ForceFromPointer)
{
	return FString::Printf(TEXT("!ChanneldUtils::CheckDifference(%s, &%s->%s())"), *GetCode_GetPropertyValueFrom(FromActor, ForceFromPointer), *FromState, *GetProtoFieldName());
}

FString FVectorPropertyDecorator::GetCode_GetProtoFieldValueFrom(const FString& StateName)
{
	return FString::Printf(TEXT("ChanneldUtils::GetVector(%s->%s())"), *StateName, *GetProtoFieldName());
}

FString FVectorPropertyDecorator::GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode)
{
	return FString::Printf(TEXT("ChanneldUtils::SetVectorToPB(%s->mutable_%s(), %s)"), *StateName, *GetProtoFieldName(), *GetValueCode);
}

FString FVectorPropertyDecorator::GetCode_SetDeltaStateArrayInner(const FString& PropertyPointer, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	FormatArgs.Add(TEXT("Declare_DeltaStateName"), DeltaStateName);
	FormatArgs.Add(TEXT("Declare_FullStateName"), FullStateName);
	FormatArgs.Add(TEXT("Definition_ProtoName"), GetProtoFieldName());
	return FString::Format(VectorPropDeco_SetDeltaStateArrayInnerTemp, FormatArgs);
}

FString FVectorPropertyDecorator::GetCode_SetPropertyValueArrayInner(const FString& PropertyPointer, const FString& NewStateName)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	return FString::Format(VectorPropDeco_OnChangeStateArrayInnerTemp, FormatArgs);
}

TArray<FString> FVectorPropertyDecorator::GetAdditionalIncludes()
{
	return TArray<FString>{TEXT("ChanneldUtils.h")};
}
