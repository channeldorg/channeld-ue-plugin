#include "PropertyDecorator/StringPropertyDecorator.h"

FString FStringPropertyDecorator::GetCode_GetProtoFieldValueFrom(const FString& StateName)
{
	return FString::Printf(TEXT("UTF8_TO_TCHAR(%s->%s().c_str())"), *StateName, *GetProtoFieldName());
}

FString FStringPropertyDecorator::GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode)
{
	return FString::Printf(TEXT("%s->set_%s(std::string(TCHAR_TO_UTF8(*%s)))"), *StateName, *GetProtoFieldName(), *GetValueCode);
}

FString FStringPropertyDecorator::GetPropertyType()
{
	return TEXT("FStrProperty");
}
