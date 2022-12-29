#include "PropertyDecorator/VectorPropertyDecorator.h"


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

FString FVectorPropertyDecorator::GetCode_GetProtoFieldValueFrom(const FString& StateName)
{
	return FString::Printf(TEXT("ChanneldUtils::GetVector(%s->%s())"), *StateName, *GetProtoFieldName());
}

FString FVectorPropertyDecorator::GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState)
{
	return TEXT("false");
}

FString FVectorPropertyDecorator::GetCode_SetDeltaState(const FString& TargetInstance, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull)
{
	return FString::Printf(
		TEXT("if (ChanneldUtils::SetIfNotSame(%s%s->mutable_%s(), *%s))\n{\n  bStateChanged = true;\n}\n"),
		ConditionFullStateIsNull ? TEXT("bIsFullStateNull ? nullptr : ") : TEXT(""),
		*DeltaStateName, *GetProtoFieldName(), *GetPointerName()
	);
}

FString FVectorPropertyDecorator::GetCode_SetDeltaStateByMemOffset(const FString& ContainerName, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull)
{
	return FString::Printf(
		TEXT("{\n%s;\nif (ChanneldUtils::SetIfNotSame(%s%s->mutable_%s(), *PropAddr))\n{\n  bStateChanged = true;\n}\n}\n"),
		*GetCode_AssignPropPointer(
			ContainerName,
			FString::Printf(TEXT("%s* PropAddr"), *GetCPPType())
		),
		ConditionFullStateIsNull ? TEXT("bIsFullStateNull ? nullptr : ") : TEXT(""),
		*DeltaStateName, *GetProtoFieldName()
	);
}

FString FVectorPropertyDecorator::GetCode_SetDeltaStateArrayInner(const FString& PropertyPointer, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	FormatArgs.Add(TEXT("Declare_DeltaStateName"), DeltaStateName);
	FormatArgs.Add(TEXT("Declare_FullStateName"), FullStateName);
	FormatArgs.Add(TEXT("Definition_ProtoName"), GetProtoFieldName());
	return FString::Format(VectorPropDeco_SetDeltaStateArrayInnerTemp, FormatArgs);
}

FString FVectorPropertyDecorator::GetCode_SetPropertyValueArrayInner(const FString& PropertyPointer, const FString& NewStateName)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	return FString::Format(VectorPropDeco_OnChangeStateArrayInnerTemp, FormatArgs);
}

TArray<FString> FVectorPropertyDecorator::GetAdditionalIncludes()
{
	return TArray<FString>{TEXT("ChanneldUtils.h")};
}
