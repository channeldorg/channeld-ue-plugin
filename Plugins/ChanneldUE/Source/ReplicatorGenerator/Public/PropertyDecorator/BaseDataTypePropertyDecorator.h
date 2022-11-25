#pragma once
#include "PropertyDecorator.h"

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

class FUInt32PropertyDecorator : public FPropertyDecorator
{
public:
	FUInt32PropertyDecorator()
	{
		ProtoFieldRule = TEXT("optional");
		ProtoFieldType = TEXT("uint32");
	}


	virtual bool IsBlueprintType() override
	{
		// TODO
		return false;
	}

	virtual bool IsExternallyAccessible() override
	{
		// TODO
		return false;
	}

	virtual FString GetCode_GetPropertyValueFrom(const FString& TargetInstanceRef) override
	{
		return FString::Printf(TEXT("%s->%s"), *TargetInstanceRef, *GetPropertyName());
	}

	virtual FString GetCode_SetPropertyValueTo(const FString& TargetInstanceRef, const FString& InValue) override
	{
		return FString::Printf(TEXT("%s->%s = %s"), *TargetInstanceRef, *GetPropertyName(), *InValue);
	}

	virtual FString GetCode_GetProtoFieldValueFrom(const FString& MessageRef) override
	{
		return FString::Printf(TEXT("%s->%s()"), *MessageRef, *GetProtoFieldName());
	}

	virtual FString GetCode_SetProtoFieldValueTo(const FString& MessageRef, const FString& InValue) override
	{
		return FString::Printf(TEXT("%s->set_%s(%s)"), *MessageRef, *GetProtoFieldName(), *InValue);
	}

	virtual FString GetCode_HasProtoFieldValueIn(const FString& MessageRef) override
	{
		return FString::Printf(TEXT("%s->has_%s"), *MessageRef, *GetProtoFieldName());
	}

	virtual FString GetCode_SetDeltaState(const FString& TargetInstanceRef, const FString& FullStateRef, const FString& DeltaStateRef) override
	{
		FStringFormatNamedArguments FormatArgs;
		const FString CodeOfGetPropertyValue = GetCode_GetPropertyValueFrom(TargetInstanceRef);
		FormatArgs.Add(TEXT("Code_GetPropertyValue"), FStringFormatArg(CodeOfGetPropertyValue));
		FormatArgs.Add(TEXT("Code_GetProtoFieldValue"), FStringFormatArg(GetCode_GetProtoFieldValueFrom(FullStateRef)));
		FormatArgs.Add(TEXT("Code_SetProtoFieldValue"), FStringFormatArg(GetCode_SetProtoFieldValueTo(DeltaStateRef, CodeOfGetPropertyValue)));

		return FString::Format(PropertyDecorator_SetDeltaStateTemplate, FormatArgs);
	}

	virtual FString GetCode_OnStateChange(const FString& TargetInstanceRef, const FString& NewStateRef) override
	{
		FStringFormatNamedArguments FormatArgs;
		const FString CodeOfGetProtoFieldValue = GetCode_GetProtoFieldValueFrom(NewStateRef);
		FormatArgs.Add(TEXT("Code_HasProtoFieldValue"), FStringFormatArg(GetCode_HasProtoFieldValueIn(NewStateRef)));
		FormatArgs.Add(TEXT("Code_GetPropertyValue"), FStringFormatArg(GetCode_GetPropertyValueFrom(TargetInstanceRef)));
		FormatArgs.Add(TEXT("Code_GetProtoFieldValue"), FStringFormatArg(CodeOfGetProtoFieldValue));
		FormatArgs.Add(
			TEXT("Code_SetPropertyValue"),
			FStringFormatArg(GetCode_SetPropertyValueTo(TargetInstanceRef, CodeOfGetProtoFieldValue))
		);

		return FString::Format(PropertyDecorator_OnChangeStateTemplate, FormatArgs);
	}
};
