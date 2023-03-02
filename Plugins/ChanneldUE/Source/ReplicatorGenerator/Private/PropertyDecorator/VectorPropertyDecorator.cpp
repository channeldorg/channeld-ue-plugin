#include "PropertyDecorator/VectorPropertyDecorator.h"


FString FVectorPropertyDecorator::GetFunctionName_SetXXXFromPB() const
{
	return TEXT("SetVectorFromPB");
}

FString FVectorPropertyDecorator::GetFunctionName_SetXXXToPB() const
{
	return TEXT("SetVectorToPB");
}

FString FVectorPropertyDecorator::GetPropertyType()
{
	return TEXT("FVector");
}

FString FVectorPropertyDecorator::GetProtoFieldType()
{
	return FString::Printf(TEXT("%s.%s"), *GetProtoPackageName(), *GetProtoStateMessageType());
}

FString FVectorPropertyDecorator::GetProtoPackageName()
{
	return TEXT("unrealpb");
}

FString FVectorPropertyDecorator::GetProtoNamespace()
{
	return GetProtoPackageName();
}

FString FVectorPropertyDecorator::GetProtoStateMessageType()
{
	return TEXT("FVector");
}

FString FVectorPropertyDecorator::GetCode_SetPropertyValueTo(const FString& TargetInstance, const FString& NewStateName, const FString& AfterSetValueCode)
{
	return FString::Printf(
		TEXT("ChanneldUtils::%s(%s, %s);\nbStateChanged = true;\n%s"),
		*GetFunctionName_SetXXXFromPB(),
		*GetCode_GetPropertyValueFrom(TargetInstance),
		*GetCode_GetProtoFieldValueFrom(NewStateName),
		*AfterSetValueCode
	);
}

FString FVectorPropertyDecorator::GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState)
{
	return FString::Printf(TEXT("!ChanneldUtils::CheckDifference(%s, &%s->%s())"), *GetCode_GetPropertyValueFrom(FromActor), *FromState, *GetProtoFieldName());
}

FString FVectorPropertyDecorator::GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState, bool ForceFromPointer)
{
	return FString::Printf(TEXT("!ChanneldUtils::CheckDifference(%s, &%s->%s())"), *GetCode_GetPropertyValueFrom(FromActor, ForceFromPointer), *FromState, *GetProtoFieldName());
}

FString FVectorPropertyDecorator::GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode)
{
	return FString::Printf(
		TEXT("ChanneldUtils::%s(%s->mutable_%s(), %s)"),
		*GetFunctionName_SetXXXToPB(),
		*StateName,
		*GetProtoFieldName(),
		*GetValueCode
	);
}

FString FVectorPropertyDecorator::GetCode_SetDeltaStateByMemOffset(const FString& ContainerName, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull)
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
	return FString::Format(VectorPropDeco_SetDeltaStateByMemOffsetTemp, FormatArgs);
}

FString FVectorPropertyDecorator::GetCode_SetDeltaStateArrayInner(const FString& PropertyPointer, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Definition_PropertyType"), GetPropertyType());
	FormatArgs.Add(TEXT("Definition_ProtoStateMessageType"), GetProtoStateMessageType());
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	FormatArgs.Add(TEXT("Declare_DeltaStateName"), DeltaStateName);
	FormatArgs.Add(TEXT("Declare_FullStateName"), FullStateName);
	FormatArgs.Add(TEXT("Definition_ProtoName"), GetProtoFieldName());
	FormatArgs.Add(TEXT("FunctionName_SetXXXToPB"), GetFunctionName_SetXXXToPB());
	return FString::Format(VectorPropDeco_SetDeltaStateArrayInnerTemp, FormatArgs);
}

FString FVectorPropertyDecorator::GetCode_OnStateChangeByMemOffset(const FString& ContainerName, const FString& NewStateName)
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
	FormatArgs.Add(TEXT("FunctionName_SetXXXFromPB"), GetFunctionName_SetXXXFromPB());
	return FString::Format(VectorPropDeco_OnChangeStateByMemOffsetTemp, FormatArgs);
}

FString FVectorPropertyDecorator::GetCode_SetPropertyValueArrayInner(const FString& PropertyPointer, const FString& NewStateName)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	FormatArgs.Add(TEXT("FunctionName_SetXXXFromPB"), GetFunctionName_SetXXXFromPB());
	return FString::Format(VectorPropDeco_OnChangeStateArrayInnerTemp, FormatArgs);
}

TArray<FString> FVectorPropertyDecorator::GetAdditionalIncludes()
{
	return TArray<FString>{TEXT("ChanneldUtils.h")};
}
