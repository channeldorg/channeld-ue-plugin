#include "PropertyDecorator/ArrayPropertyDecorator.h"

#include "PropertyDecoratorFactory.h"

FArrayPropertyDecorator::FArrayPropertyDecorator(FProperty* InProperty, IPropertyDecoratorOwner* InOwner)
	: FPropertyDecorator(InProperty, InOwner)
{
	check(InProperty != nullptr)
	FPropertyDecoratorFactory& Factory = FPropertyDecoratorFactory::Get();
	InnerProperty = Factory.GetPropertyDecorator(CastFieldChecked<FArrayProperty>(InProperty)->Inner, this);
	ProtoFieldRule = TEXT("repeated");
	ProtoFieldType = InnerProperty->GetProtoFieldType();
}

FString FArrayPropertyDecorator::GetCPPType()
{
	return FString::Printf(TEXT("TArray<%s>"), *InnerProperty->GetCompilableCPPType());
}

FString FArrayPropertyDecorator::GetPropertyType()
{
	return TEXT("FArrayProperty");
}

FString FArrayPropertyDecorator::GetCode_SetDeltaState(const FString& TargetInstance, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropPtrName"), GetPointerName());
	FormatArgs.Add(TEXT("Code_GetProtoFieldValueFrom"), GetCode_GetProtoFieldValueFrom(TEXT("FullState")));
	FormatArgs.Add(
		TEXT("Code_SetDeltaStateArrayInner"),
		InnerProperty->GetCode_SetDeltaStateArrayInner(TargetInstance, FullStateName, DeltaStateName, ConditionFullStateIsNull)
	);
	return FString::Format(ArrPropDecorator_SetDeltaStateTemplate, FormatArgs);
}

FString FArrayPropertyDecorator::GetCode_HasProtoFieldValueIn(const FString& StateName)
{
	return FString::Printf(TEXT("%s.size() > 0"), *GetCode_GetProtoFieldValueFrom(StateName));
}

FString FArrayPropertyDecorator::GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState)
{
	return TEXT("false");
}

FString FArrayPropertyDecorator::GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState, bool ForceFromPointer)
{
	return TEXT("false");
}

FString FArrayPropertyDecorator::GetCode_SetPropertyValueTo(const FString& TargetInstance, const FString& NewStataName, const FString& AfterSetValueCode)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropPtrName"), GetPointerName());
	FormatArgs.Add(TEXT("Code_GetProtoFieldValueFrom"), GetCode_GetProtoFieldValueFrom(NewStataName));
	FormatArgs.Add(TEXT("Code_SetPropertyValueArrayInner"), InnerProperty->GetCode_SetPropertyValueArrayInner(TEXT("this"), NewStataName));
	
	return FString::Format(ArrPropDecorator_SetPropertyValueTemp, FormatArgs);
}
