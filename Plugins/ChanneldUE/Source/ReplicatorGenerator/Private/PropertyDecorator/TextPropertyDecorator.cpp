#include "PropertyDecorator/TextPropertyDecorator.h"

FString FTextPropertyDecorator::GetCode_GetProtoFieldValueFrom(const FString& StateName)
{
	return FString::Printf(TEXT("FText::FromString(UTF8_TO_TCHAR(%s->%s().c_str()))"), *StateName, *GetProtoFieldName());
}

FString FTextPropertyDecorator::GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode)
{
	return FString::Printf(TEXT("%s->set_%s(std::string(TCHAR_TO_UTF8(*(%s).ToString())))"), *StateName, *GetProtoFieldName(), *GetValueCode);
}

FString FTextPropertyDecorator::GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState)
{
	return FString::Printf(TEXT("(%s).EqualTo(%s)"), *GetCode_GetPropertyValueFrom(FromActor), *GetCode_GetProtoFieldValueFrom(FromState));
}

FString FTextPropertyDecorator::GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState, bool ForceFromPointer)
{
	return FPropertyDecorator::GetCode_ActorPropEqualToProtoState(FromActor, FromState);
}

FString FTextPropertyDecorator::GetPropertyType()
{
	return TEXT("FTextProperty");
}
