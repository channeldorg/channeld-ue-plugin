#include "PropertyDecorator/RotatorPropertyDecorator.h"

#include "ChanneldUtils.h"

FString FRotatorPropertyDecorator::GetPropertyType()
{
	return TEXT("FRotator");
}

FString FRotatorPropertyDecorator::GetFunctionName_SetXXXFromPB() const
{
	return TEXT("SetRotatorFromPB");
}

FString FRotatorPropertyDecorator::GetFunctionName_SetXXXToPB() const
{
	return TEXT("SetRotatorToPB");
}
