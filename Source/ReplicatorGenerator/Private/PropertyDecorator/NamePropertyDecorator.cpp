#include "PropertyDecorator/NamePropertyDecorator.h"

FString FNamePropertyDecorator::GetPropertyType()
{
	return TEXT("FNameProperty");
}

FString FNamePropertyDecorator::GetCode_GetProtoFieldValueFrom(const FString& StateName)
{
	return FString::Printf(TEXT("FName(UTF8_TO_TCHAR(%s->%s().c_str()))"), *StateName, *GetProtoFieldName());
}

FString FNamePropertyDecorator::GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode)
{
	return FString::Printf(TEXT("%s->set_%s(std::string(TCHAR_TO_UTF8(*(%s).ToString())))"), *StateName, *GetProtoFieldName(), *GetValueCode);
}

FString FNamePropertyDecorator::GetCode_SetDeltaStateArrayInner(const FString& PropertyPointer, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	FormatArgs.Add(TEXT("Declare_DeltaStateName"), DeltaStateName);
	FormatArgs.Add(TEXT("Declare_FullStateName"), FullStateName);
	FormatArgs.Add(TEXT("Definition_ProtoName"), GetProtoFieldName());
	return FString::Format(NamePropDeco_SetDeltaStateArrayInnerTemp, FormatArgs);
}

FString FNamePropertyDecorator::GetCode_SetPropertyValueArrayInner(const FString& PropertyPointer, const FString& NewStateName)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	return FString::Format(NamePropDeco_OnChangeStateArrayInnerTemp, FormatArgs);
}
