#include "PropertyDecorator/TextPropertyDecorator.h"

FString FTextPropertyDecorator::GetPropertyType()
{
	return TEXT("FTextProperty");
}

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

FString FTextPropertyDecorator::GetCode_SetDeltaStateByMemOffset(const FString& ContainerName, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(
		TEXT("Code_AssignPropPointers"),
		GetCode_AssignPropPointer(
			ContainerName,
			FString::Printf(TEXT("%s* PropAddr"), *GetCPPType())
		)
	);
	FormatArgs.Add(TEXT("Code_BeforeCondition"), ConditionFullStateIsNull ? TEXT("bIsFullStateNull ? true :") : TEXT(""));
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), TEXT("PropAddr"));
	FormatArgs.Add(TEXT("Code_GetProtoFieldValue"), GetCode_GetProtoFieldValueFrom(FullStateName));
	FormatArgs.Add(TEXT("Code_SetProtoFieldValue"), GetCode_SetProtoFieldValueTo(DeltaStateName, TEXT("*PropAddr")));
	return FString::Format(TextPropDeco_SetDeltaStateByMemOffsetTemp, FormatArgs);
}

FString FTextPropertyDecorator::GetCode_SetDeltaStateArrayInner(const FString& PropertyPointer, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	FormatArgs.Add(TEXT("Declare_DeltaStateName"), DeltaStateName);
	FormatArgs.Add(TEXT("Declare_FullStateName"), FullStateName);
	FormatArgs.Add(TEXT("Definition_ProtoName"), GetProtoFieldName());
	return FString::Format(TextPropDeco_SetDeltaStateArrayInnerTemp, FormatArgs);
}

FString FTextPropertyDecorator::GetCode_OnStateChangeByMemOffset(const FString& ContainerName, const FString& NewStateName)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(
		TEXT("Code_AssignPropPointers"),
		GetCode_AssignPropPointer(
			ContainerName,
			FString::Printf(TEXT("%s* PropAddr"), *GetCPPType())
		)
	);
	FormatArgs.Add(TEXT("Code_HasProtoFieldValue"), GetCode_HasProtoFieldValueIn(NewStateName));
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), TEXT("PropAddr"));
	FormatArgs.Add(TEXT("Code_GetProtoFieldValue"), GetCode_GetProtoFieldValueFrom(NewStateName));
	return FString::Format(TextPropDeco_OnChangeStateByMemOffsetTemp, FormatArgs);
}
FString FTextPropertyDecorator::GetCode_SetPropertyValueArrayInner(const FString& PropertyPointer, const FString& NewStateName)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	return FString::Format(TextPropDeco_OnChangeStateArrayInnerTemp, FormatArgs);
}
