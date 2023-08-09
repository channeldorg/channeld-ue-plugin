#include "PropertyDecorator/AssetPropertyDecorator.h"

FString FAssetPropertyDecorator::GetCPPType()
{
	return TEXT("UObject*");
}

FString FAssetPropertyDecorator::GetPropertyType()
{
	return TEXT("FObjectProperty");
}

FString FAssetPropertyDecorator::GetProtoFieldType()
{
	return FString::Printf(TEXT("%s.%s"), *GetProtoPackageName(), *GetProtoStateMessageType());
}

FString FAssetPropertyDecorator::GetProtoPackageName()
{
	return TEXT("unrealpb");
}

FString FAssetPropertyDecorator::GetProtoNamespace()
{
	return GetProtoPackageName();
}

FString FAssetPropertyDecorator::GetProtoStateMessageType()
{
	return TEXT("AssetRef");
}

FString FAssetPropertyDecorator::GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState)
{
	return FString::Printf(
		TEXT("%s == ChanneldUtils::GetAssetByRef(&%s->%s())"),
		*GetCode_GetPropertyValueFrom(FromActor),
		*FromState, *GetProtoFieldName()
	);
}

FString FAssetPropertyDecorator::GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState, bool ForceFromPointer)
{
	return FString::Printf(
		TEXT("%s == ChanneldUtils::GetAssetByRef(&%s->%s())"),
		*GetCode_GetPropertyValueFrom(FromActor, ForceFromPointer),
		*FromState, *GetProtoFieldName()
	);
}

FString FAssetPropertyDecorator::GetCode_GetProtoFieldValueFrom(const FString& StateName)
{
	return FString::Printf(
		TEXT("ChanneldUtils::GetAssetByRef(&%s->%s())"),
		*StateName, *GetProtoFieldName()
	);
}

FString FAssetPropertyDecorator::GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode)
{
	return FString::Printf(TEXT("%s->mutable_%s()->CopyFrom(ChanneldUtils::GetAssetRef(%s))"), *StateName, *GetProtoFieldName(), *GetValueCode);
}

FString FAssetPropertyDecorator::GetCode_SetPropertyValueTo(const FString& TargetInstance, const FString& NewStateName, const FString& AfterSetValueCode)
{
	return FString::Printf(
		TEXT("%s = ChanneldUtils::GetAssetByRef(&%s->%s());\n  bStateChanged = true;\n%s"),
		*GetCode_GetPropertyValueFrom(TargetInstance),
		*NewStateName, *GetProtoFieldName(),
		*AfterSetValueCode
	);
}

FString FAssetPropertyDecorator::GetCode_SetDeltaStateArrayInner(const FString& PropertyPointer, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	FormatArgs.Add(TEXT("Declare_DeltaStateName"), DeltaStateName);
	FormatArgs.Add(TEXT("Declare_FullStateName"), FullStateName);
	FormatArgs.Add(TEXT("Definition_ProtoName"), GetProtoFieldName());
	return FString::Format(UAssetPropDeco_SetDeltaStateArrayInnerTemp, FormatArgs);
}

FString FAssetPropertyDecorator::GetCode_SetPropertyValueArrayInner(const FString& PropertyPointer, const FString& NewStateName)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	return FString::Format(UAssetPropDeco_OnChangeStateArrayInnerTemp, FormatArgs);
}

TArray<FString> FAssetPropertyDecorator::GetAdditionalIncludes()
{
	return TArray<FString>{TEXT("ChanneldUtils.h")};
}

FString FAssetPropertyDecorator::GetCode_GetWorldRef()
{
	return TEXT("");
}
