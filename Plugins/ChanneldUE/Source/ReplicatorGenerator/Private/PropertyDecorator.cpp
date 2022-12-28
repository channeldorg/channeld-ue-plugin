#include "PropertyDecorator.h"

bool FPropertyDecorator::Init()
{
	return true;
}

bool FPropertyDecorator::IsBlueprintType()
{
	return false;
}

bool FPropertyDecorator::IsExternallyAccessible()
{
	return OriginalProperty->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPublic);
}

bool FPropertyDecorator::IsDirectlyAccessible()
{
	return !bForceNotDirectlyAccessible && IsExternallyAccessible() && !Owner->IsBlueprintType();
}

bool FPropertyDecorator::IsForceNotDirectlyAccessible() const
{
	return bForceNotDirectlyAccessible;
}

void FPropertyDecorator::SetForceNotDirectlyAccessible(bool ForceNotDirectlyAccessible)
{
	this->bForceNotDirectlyAccessible = ForceNotDirectlyAccessible;
}

bool FPropertyDecorator::IsDeclaredInCPP()
{
	return true;
}

FString FPropertyDecorator::GetPropertyName()
{
	return OriginalProperty->GetName();
}

FString FPropertyDecorator::GetPointerName()
{
	return GetPropertyName() + TEXT("Ptr");
}

FString FPropertyDecorator::GetCPPType()
{
	return OriginalProperty->GetCPPType();
}

FString FPropertyDecorator::GetCompilableCPPType()
{
	return GetCPPType();
}

int32 FPropertyDecorator::GetMemOffset()
{
	return OriginalProperty->GetOffset_ForInternal();
}

int32 FPropertyDecorator::GetPropertySize()
{
	return OriginalProperty->ElementSize;
}

int32 FPropertyDecorator::GetPropertyMinAlignment()
{
	return OriginalProperty->GetMinAlignment();
}

FString FPropertyDecorator::GetProtoPackageName()
{
	return Owner->GetProtoPackageName();
}

FString FPropertyDecorator::GetProtoNamespace()
{
	return Owner->GetProtoNamespace();
}

FString FPropertyDecorator::GetProtoStateMessageType()
{
	return Owner->GetProtoStateMessageType();
}

FString FPropertyDecorator::GetProtoFieldName()
{
	return GetPropertyName().ToLower();
}

FString FPropertyDecorator::GetDefinition_ProtoField()
{
	const FString& FieldRule = GetProtoFieldRule();
	const FString SpaceChar = TEXT(" ");
	return FString(FieldRule.IsEmpty() ? TEXT("") : FieldRule + SpaceChar) + GetProtoFieldType() + SpaceChar + GetProtoFieldName();
}

FString FPropertyDecorator::GetDefinition_ProtoField(int32 FieldNumber)
{
	return GetDefinition_ProtoField() + TEXT(" = ") + FString::FromInt(FieldNumber);
}

FString FPropertyDecorator::GetCode_GetPropertyValueFrom(const FString& TargetInstance)
{
	return GetCode_GetPropertyValueFrom(TargetInstance, !IsDirectlyAccessible());
}

FString FPropertyDecorator::GetCode_GetPropertyValueFrom(const FString& TargetInstance, bool ForceFromPointer/* = false */)
{
	if (!ForceFromPointer)
	{
		return FString::Printf(TEXT("%s->%s"), *TargetInstance, *GetPropertyName());
	}
	else
	{
		return TEXT("*") + GetPointerName();
	}
}

FString FPropertyDecorator::GetCode_SetPropertyValueTo(const FString& TargetInstance, const FString& NewStateName, const FString& AfterSetValueCode)
{
	return FString::Printf(TEXT("%s = %s;\nbStateChanged = true;\n%s"), *GetCode_GetPropertyValueFrom(TargetInstance), *GetCode_GetProtoFieldValueFrom(NewStateName), *AfterSetValueCode);
}

FString FPropertyDecorator::GetDeclaration_PropertyPtr()
{
	return FString::Printf(TEXT("%s* %s"), *this->GetCPPType(), *this->GetPointerName());
}

FString FPropertyDecorator::GetCode_AssignPropPointer(const FString& Container, const FString& AssignTo)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Ref_AssignTo"), AssignTo);
	FormatArgs.Add(TEXT("Ref_ContainerAddr"), Container);
	FormatArgs.Add(TEXT("Declare_PropertyCPPType"), GetCPPType());
	FormatArgs.Add(TEXT("Num_PropMemOffset"), GetMemOffset());

	return FString::Format(PropDecorator_AssignPropPtrTemp, FormatArgs);
}

FString FPropertyDecorator::GetCode_AssignPropPtrDispersedly(const FString& Container, const FString& ContainerTemplate, const FString& AssignTo)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Ref_AssignTo"), AssignTo);
	FormatArgs.Add(TEXT("Ref_ContainerAddr"), Container);
	FormatArgs.Add(TEXT("Ref_ContainerTemplate"), ContainerTemplate);
	FormatArgs.Add(TEXT("Declare_PropertyCPPType"), GetCPPType());
	FormatArgs.Add(TEXT("Declare_PropertyName"), GetPropertyName());

	return FString::Format(PropDecorator_AssignPropPtrDispersedlyTemp, FormatArgs);
}

FString FPropertyDecorator::GetCode_AssignPropPtrOrderly(const FString& Container, const FString& ContainerTemplate, const FString& AssignTo)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Ref_AssignTo"), AssignTo);
	FormatArgs.Add(TEXT("Ref_ContainerAddr"), Container);
	FormatArgs.Add(TEXT("Ref_ContainerTemplate"), ContainerTemplate);
	FormatArgs.Add(TEXT("Declare_PropertyCPPType"), GetCPPType());

	return FString::Format(PropDecorator_AssignPropPtrOrderlyTemp, FormatArgs);
}

FString FPropertyDecorator::GetCode_GetProtoFieldValueFrom(const FString& StateName)
{
	return FString::Printf(TEXT("%s->%s()"), *StateName, *GetProtoFieldName());
}

FString FPropertyDecorator::GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode)
{
	return FString::Printf(TEXT("%s->set_%s(%s)"), *StateName, *GetProtoFieldName(), *GetValueCode);
}

FString FPropertyDecorator::GetCode_HasProtoFieldValueIn(const FString& StateName)
{
	return FString::Printf(TEXT("%s->has_%s()"), *StateName, *GetProtoFieldName());
}

FString FPropertyDecorator::GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState)
{
	return FString::Printf(TEXT("%s == %s"), *GetCode_GetPropertyValueFrom(FromActor), *GetCode_GetProtoFieldValueFrom(FromState));
}

FString FPropertyDecorator::GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState, bool ForceFromPointer)
{
	return FString::Printf(TEXT("%s == %s"), *GetCode_GetPropertyValueFrom(FromActor, ForceFromPointer), *GetCode_GetProtoFieldValueFrom(FromState));
}

FString FPropertyDecorator::GetCode_SetDeltaState(const FString& TargetInstance, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Code_BeforeCondition"), ConditionFullStateIsNull ? TEXT("bIsFullStateNull ? true :") : TEXT(""));
	FormatArgs.Add(TEXT("Code_ActorPropEqualToProtoState"), GetCode_ActorPropEqualToProtoState(TargetInstance, FullStateName));
	FormatArgs.Add(TEXT("Code_SetProtoFieldValue"), GetCode_SetProtoFieldValueTo(DeltaStateName, GetCode_GetPropertyValueFrom(TargetInstance)));
	return FString::Format(PropDecorator_SetDeltaStateTemplate, FormatArgs);
}

FString FPropertyDecorator::GetCode_SetDeltaStateByMemOffset(const FString& ContainerName, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull)
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
	return FString::Format(PropDeco_SetDeltaStateByMemOffsetTemp, FormatArgs);
}

FString FPropertyDecorator::GetCode_SetDeltaStateArrayInner(const FString& PropertyPointer, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull)
{
	FStringFormatNamedArguments FormatArgs;
	// FormatArgs.Add(TEXT("Code_BeforeCondition"), ConditionFullStateIsNull ? TEXT("bIsFullStateNull ? true :") : TEXT(""));
	FormatArgs.Add(TEXT("Declare_DeltaStateName"), DeltaStateName);
	FormatArgs.Add(TEXT("Declare_FullStateName"), FullStateName);
	FormatArgs.Add(TEXT("Definition_ProtoName"), GetProtoFieldName());
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	return FString::Format(PropDeco_SetDeltaStateArrayInnerTemp, FormatArgs);
}

FString FPropertyDecorator::GetCode_OnStateChange(const FString& TargetInstance, const FString& NewStateName)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Code_HasProtoFieldValue"), GetCode_HasProtoFieldValueIn(NewStateName));
	FormatArgs.Add(TEXT("Code_ActorPropEqualToProtoState"), GetCode_ActorPropEqualToProtoState(TargetInstance, NewStateName));
	FormatArgs.Add(
		TEXT("Code_SetPropertyValue"),
		FStringFormatArg(GetCode_SetPropertyValueTo(TargetInstance, NewStateName, TEXT("")))
	);
	return FString::Format(PropDecorator_OnChangeStateTemplate, FormatArgs);
}

FString FPropertyDecorator::GetCode_OnStateChangeByMemOffset(const FString& ContainerName, const FString& NewStateName)
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
	return FString::Format(PropDeco_OnChangeStateByMemOffsetTemp, FormatArgs);
}

FString FPropertyDecorator::GetCode_SetPropertyValueArrayInner(const FString& PropertyPointer, const FString& NewStateName)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	return FString::Format(PropDeco_OnChangeStateArrayInnerTemp, FormatArgs);
}

TArray<FString> FPropertyDecorator::GetAdditionalIncludes()
{
	return TArray<FString>();
}
