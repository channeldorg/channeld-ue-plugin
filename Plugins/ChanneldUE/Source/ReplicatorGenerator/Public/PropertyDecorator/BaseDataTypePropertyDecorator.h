#pragma once
#include "PropertyDecorator.h"
#include "PropertyDecoratorBuilder/BaseDataTypePropertyDecoratorBuilder.h"

#define BASE_DATA_TYPE_PROPERTY_DECORATOR_BASE(ClassName, PropertyType, CPPType, ProtoType) \
class ClassName : public FPropertyDecorator \
{ \
public: \
	ClassName() \
	{ \
		ProtoFieldRule = TEXT("optional"); \
		ProtoFieldType = TEXT(#ProtoType); \
	} \
	virtual FString GetCPPType() override \
	{ \
		return TEXT(#CPPType); \
	} \
	virtual FString GetPropertyType() override \
	{ \
		return TEXT(#PropertyType); \
	} \
	virtual FString GetCode_SetPropertyValueTo(const FString& TargetInstanceRef, const FString& InValue) override \
	{ \
		return FString::Printf(TEXT("%s = %s"), *GetCode_GetPropertyValueFrom(TargetInstanceRef), *InValue); \
	} \
	virtual FString GetCode_SetDeltaState(const FString& TargetInstanceRef, const FString& FullStateRef, const FString& DeltaStateRef) override \
	{ \
		FStringFormatNamedArguments FormatArgs; \
		const FString CodeOfGetPropertyValue = GetCode_GetPropertyValueFrom(TargetInstanceRef); \
		FormatArgs.Add(TEXT("Code_GetPropertyValue"), FStringFormatArg(CodeOfGetPropertyValue)); \
		FormatArgs.Add(TEXT("Code_GetProtoFieldValue"), FStringFormatArg(GetCode_GetProtoFieldValueFrom(FullStateRef))); \
		FormatArgs.Add(TEXT("Code_SetProtoFieldValue"), FStringFormatArg(GetCode_SetProtoFieldValueTo(DeltaStateRef, CodeOfGetPropertyValue))); \
		return FString::Format(PropertyDecorator_SetDeltaStateTemplate, FormatArgs); \
	} \
	virtual FString GetCode_OnStateChange(const FString& TargetInstanceRef, const FString& NewStateRef) override \
	{ \
		FStringFormatNamedArguments FormatArgs; \
		const FString CodeOfGetProtoFieldValue = GetCode_GetProtoFieldValueFrom(NewStateRef); \
		FormatArgs.Add(TEXT("Code_HasProtoFieldValue"), FStringFormatArg(GetCode_HasProtoFieldValueIn(NewStateRef))); \
		FormatArgs.Add(TEXT("Code_GetPropertyValue"), FStringFormatArg(GetCode_GetPropertyValueFrom(TargetInstanceRef))); \
		FormatArgs.Add(TEXT("Code_GetProtoFieldValue"), FStringFormatArg(CodeOfGetProtoFieldValue)); \
		FormatArgs.Add( \
			TEXT("Code_SetPropertyValue"), \
			FStringFormatArg(GetCode_SetPropertyValueTo(TargetInstanceRef, CodeOfGetProtoFieldValue)) \
		); \
		return FString::Format(PropertyDecorator_OnChangeStateTemplate, FormatArgs); \
	} \
}

#define BASE_DATA_TYPE_PROPERTY_DECORATOR(ClassName, PropertyType, CPPType) \
	BASE_DATA_TYPE_PROPERTY_DECORATOR_BASE(ClassName, PropertyType, CPPType, CPPType)

#define BASE_DATA_TYPE_PROPERTY_DECORATOR_WITH_BUILDER_BASE(BuilderClassName, DecoratorClassName, PropertyType, CPPType, ProtoType) \
	BASE_DATA_TYPE_PROPERTY_DECORATOR_BASE(DecoratorClassName, PropertyType, CPPType, ProtoType); \
	BASE_DATA_TYPE_PROPERTY_DECORATOR_BUILDER(BuilderClassName, DecoratorClassName, PropertyType)

#define BASE_DATA_TYPE_PROPERTY_DECORATOR_WITH_BUILDER(BuilderClassName, DecoratorClassName, PropertyType, CPPType) \
	BASE_DATA_TYPE_PROPERTY_DECORATOR_WITH_BUILDER_BASE(BuilderClassName, DecoratorClassName, PropertyType, CPPType, CPPType)

const static TCHAR* PropertyDecorator_SetDeltaStateTemplate =
	LR"EOF(
if ({Code_GetPropertyValue} != {Code_GetProtoFieldValue})
{
  {Code_SetProtoFieldValue};
  bStateChanged = true;
}
)EOF";

const static TCHAR* PropertyDecorator_OnChangeStateTemplate =
	LR"EOF(
if ({Code_HasProtoFieldValue} && {Code_GetPropertyValue} != {Code_GetProtoFieldValue})
{
  {Code_SetPropertyValue};
}
)EOF";

BASE_DATA_TYPE_PROPERTY_DECORATOR_WITH_BUILDER_BASE(FBytePropertyDecoratorBuilder, FBytePropertyDecorator, FByteProperty, uint8, uint32);
BASE_DATA_TYPE_PROPERTY_DECORATOR_WITH_BUILDER(FBoolPropertyDecoratorBuilder, FBoolPropertyDecorator, FBoolProperty, bool);
BASE_DATA_TYPE_PROPERTY_DECORATOR_WITH_BUILDER(FUInt32PropertyDecoratorBuilder, FUInt32PropertyDecorator, FUInt32Property, uint32);
BASE_DATA_TYPE_PROPERTY_DECORATOR_WITH_BUILDER(FIntPropertyDecoratorBuilder, FIntPropertyDecorator, FIntProperty, int32);
BASE_DATA_TYPE_PROPERTY_DECORATOR_WITH_BUILDER(FUInt64PropertyDecoratorBuilder, FUInt64PropertyDecorator, FUInt64Property, uint64);
BASE_DATA_TYPE_PROPERTY_DECORATOR_WITH_BUILDER(FInt64PropertyDecoratorBuilder, FInt64PropertyDecorator, FInt64Property, int64);
BASE_DATA_TYPE_PROPERTY_DECORATOR_WITH_BUILDER(FFloatPropertyDecoratorBuilder, FFloatPropertyDecorator, FFloatProperty, float);
BASE_DATA_TYPE_PROPERTY_DECORATOR_WITH_BUILDER(FDoublePropertyDecoratorBuilder, FDoublePropertyDecorator, FDoubleProperty, double);

//
// class FUInt32PropertyDecorator : public FPropertyDecorator
// {
// public:
//
//
// 	FUInt32PropertyDecorator()
// 	{
// 		ProtoFieldRule = TEXT("optional");
// 		ProtoFieldType = TEXT("uint32");
// 	}
//
// 	virtual FString GetCPPType() override
// 	{
// 		return TEXT("uint32");
// 	}
//
// 	virtual FString GetPropertyType() override
// 	{
// 		return TEXT("FUInt32Property");
// 	}
//
// 	virtual FString GetCode_SetPropertyValueTo(const FString& TargetInstanceRef, const FString& InValue) override
// 	{
// 		return FString::Printf(TEXT("%s = %s"), *GetCode_GetPropertyValueFrom(TargetInstanceRef), *InValue);
// 	}
//
// 	virtual FString GetCode_SetDeltaState(const FString& TargetInstanceRef, const FString& FullStateRef, const FString& DeltaStateRef) override
// 	{
// 		FStringFormatNamedArguments FormatArgs;
// 		const FString CodeOfGetPropertyValue = GetCode_GetPropertyValueFrom(TargetInstanceRef);
// 		FormatArgs.Add(TEXT("Code_GetPropertyValue"), FStringFormatArg(CodeOfGetPropertyValue));
// 		FormatArgs.Add(TEXT("Code_GetProtoFieldValue"), FStringFormatArg(GetCode_GetProtoFieldValueFrom(FullStateRef)));
// 		FormatArgs.Add(TEXT("Code_SetProtoFieldValue"), FStringFormatArg(GetCode_SetProtoFieldValueTo(DeltaStateRef, CodeOfGetPropertyValue)));
//
// 		return FString::Format(PropertyDecorator_SetDeltaStateTemplate, FormatArgs);
// 	}
//
// 	virtual FString GetCode_OnStateChange(const FString& TargetInstanceRef, const FString& NewStateRef) override
// 	{
// 		FStringFormatNamedArguments FormatArgs;
// 		const FString CodeOfGetProtoFieldValue = GetCode_GetProtoFieldValueFrom(NewStateRef);
// 		FormatArgs.Add(TEXT("Code_HasProtoFieldValue"), FStringFormatArg(GetCode_HasProtoFieldValueIn(NewStateRef)));
// 		FormatArgs.Add(TEXT("Code_GetPropertyValue"), FStringFormatArg(GetCode_GetPropertyValueFrom(TargetInstanceRef)));
// 		FormatArgs.Add(TEXT("Code_GetProtoFieldValue"), FStringFormatArg(CodeOfGetProtoFieldValue));
// 		FormatArgs.Add(
// 			TEXT("Code_SetPropertyValue"),
// 			FStringFormatArg(GetCode_SetPropertyValueTo(TargetInstanceRef, CodeOfGetProtoFieldValue))
// 		);
//
// 		return FString::Format(PropertyDecorator_OnChangeStateTemplate, FormatArgs);
// 	}
// };
