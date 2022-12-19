#include "PropertyDecorator/NamePropertyDecorator.h"

FString FNamePropertyDecorator::GetCode_GetProtoFieldValueFrom(const FString& StateName)
{
	return FString::Printf(TEXT("FName(UTF8_TO_TCHAR(%s->%s().c_str()))"), *StateName, *GetProtoFieldName());
}

FString FNamePropertyDecorator::GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode)
{
	return FString::Printf(TEXT("%s->set_%s(std::string(TCHAR_TO_UTF8(*(%s).ToString())))"), *StateName, *GetProtoFieldName(), *GetValueCode);
}

FString FNamePropertyDecorator::GetPropertyType()
{
	return TEXT("FNameProperty");
}
