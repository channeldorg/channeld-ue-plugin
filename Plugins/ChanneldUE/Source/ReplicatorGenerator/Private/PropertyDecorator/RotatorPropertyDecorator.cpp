#include "PropertyDecorator/RotatorPropertyDecorator.h"

FString FRotatorPropertyDecorator::GetPropertyType()
{
	return TEXT("FRotator");
}

FString FRotatorPropertyDecorator::GetCode_GetProtoFieldValueFrom(const FString& StateName)
{
	return FString::Printf(TEXT("ChanneldUtils::GetRotator(%s->%s())"), *StateName, *GetProtoFieldName());
}

FString FRotatorPropertyDecorator::GetCode_SetDeltaStateArrayInner(const FString& PropertyPointer, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	FormatArgs.Add(TEXT("Declare_DeltaStateName"), DeltaStateName);
	FormatArgs.Add(TEXT("Declare_FullStateName"), FullStateName);
	FormatArgs.Add(TEXT("Definition_ProtoName"), GetProtoFieldName());
	return FString::Format(RotatorPropDeco_SetDeltaStateArrayInnerTemp, FormatArgs);
}

FString FRotatorPropertyDecorator::GetCode_SetPropertyValueArrayInner(const FString& PropertyPointer, const FString& NewStateName)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	return FString::Format(RotatorPropDeco_OnChangeStateArrayInnerTemp, FormatArgs);
}
