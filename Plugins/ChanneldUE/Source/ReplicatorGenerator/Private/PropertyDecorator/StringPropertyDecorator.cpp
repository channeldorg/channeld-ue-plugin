#include "PropertyDecorator/StringPropertyDecorator.h"

FString FStringPropertyDecorator::GetPropertyType()
{
	return TEXT("FStrProperty");
}

FString FStringPropertyDecorator::GetCode_GetProtoFieldValueFrom(const FString& StateName)
{
	return FString::Printf(TEXT("UTF8_TO_TCHAR(%s->%s().c_str())"), *StateName, *GetProtoFieldName());
}

FString FStringPropertyDecorator::GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode)
{
	return FString::Printf(TEXT("%s->set_%s(std::string(TCHAR_TO_UTF8(*%s)))"), *StateName, *GetProtoFieldName(), *GetValueCode);
}

FString FStringPropertyDecorator::GetCode_SetDeltaStateArrayInner(const FString& PropertyPointer, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	FormatArgs.Add(TEXT("Declare_DeltaStateName"), DeltaStateName);
	FormatArgs.Add(TEXT("Declare_FullStateName"), FullStateName);
	FormatArgs.Add(TEXT("Definition_ProtoName"), GetProtoFieldName());
	return FString::Format(StrPropDeco_SetDeltaStateArrayInnerTemp, FormatArgs);
}

FString FStringPropertyDecorator::GetCode_SetPropertyValueArrayInner(const FString& PropertyPointer, const FString& NewStateName)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	return FString::Format(StrPropDeco_OnChangeStateArrayInnerTemp, FormatArgs);
}
