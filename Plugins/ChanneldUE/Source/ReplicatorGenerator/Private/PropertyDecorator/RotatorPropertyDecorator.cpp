#include "PropertyDecorator/RotatorPropertyDecorator.h"

FString FRotatorPropertyDecorator::GetPropertyType()
{
	return TEXT("FRotator");
}

FString FRotatorPropertyDecorator::GetCode_GetProtoFieldValueFrom(const FString& StateName)
{
	return FString::Printf(TEXT("ChanneldUtils::GetRotator(%s->%s())"), *StateName, *GetProtoFieldName());
}
