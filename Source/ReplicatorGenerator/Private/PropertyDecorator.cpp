﻿#include "PropertyDecorator.h"

#include "ReplicatorGeneratorUtils.h"

FPropertyDecorator::FPropertyDecorator(FProperty* InProperty, IPropertyDecoratorOwner* InOwner)
	: Owner(InOwner), OriginalProperty(InProperty)
{
}

bool FPropertyDecorator::Init(const TFunction<FString()>& SetNameForIllegalPropName)
{
	if (bInitialized)
	{
		return false;
	}

	if (OriginalProperty != nullptr)
	{
		CompilablePropName = OriginalProperty->GetName();
		if (ChanneldReplicatorGeneratorUtils::ContainsUncompilableChar(CompilablePropName))
		{
			CompilablePropName = *SetNameForIllegalPropName();
		}
	}

	PostInit();
	bInitialized = true;
	return true;
}

void FPropertyDecorator::PostInit()
{
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
	return !IsForceNotDirectlyAccessible() && IsExternallyAccessible() && !Owner->IsBlueprintType();
}

bool FPropertyDecorator::IsForceNotDirectlyAccessible() const
{
	return bForceNotDirectlyAccessible;
}

void FPropertyDecorator::SetForceNotDirectlyAccessible(bool ForceNotDirectlyAccessible)
{
	this->bForceNotDirectlyAccessible = ForceNotDirectlyAccessible;
}

bool FPropertyDecorator::HasAnyPropertyFlags(EPropertyFlags PropertyFlags)
{
	if (OriginalProperty == nullptr)
	{
		return false;
	}
	return (OriginalProperty->PropertyFlags & PropertyFlags) != 0;
}

FString FPropertyDecorator::GetPropertyName()
{
	return CompilablePropName;
}

FString FPropertyDecorator::GetPointerName()
{
	return FString::Printf(TEXT("Prop_%s_Ptr"), *GetPropertyName());
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
	return FString::Printf(TEXT("prop_%s"), *GetPropertyName().ToLower());
}

UFunction* FPropertyDecorator::FindFunctionByName(const FName& FuncName)
{
	return Owner->FindFunctionByName(FuncName);
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
	return FString::Printf(TEXT("%s = %s;\n%s"), *GetCode_GetPropertyValueFrom(TargetInstance), *GetCode_GetProtoFieldValueFrom(NewStateName), *AfterSetValueCode);
}

FString FPropertyDecorator::GetDeclaration_PropertyPtr()
{
	return FString::Printf(TEXT("%s* %s"), *this->GetCPPType(), *this->GetPointerName());
}

FString FPropertyDecorator::GetCode_AssignPropPointerStatic(const FString& Container, const FString& AssignTo)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Ref_AssignTo"), AssignTo);
	FormatArgs.Add(TEXT("Ref_ContainerAddr"), Container);
	FormatArgs.Add(TEXT("Declare_PropertyCPPType"), GetCPPType());
	FormatArgs.Add(TEXT("Num_PropMemOffset"), GetMemOffset());
	FormatArgs.Add(TEXT("Declare_PropertyName"), GetPropertyName());

	return FString::Format(PropDecorator_AssignPropPtrStatic, FormatArgs);
}

FString FPropertyDecorator::GetCode_AssignPropPointerDynamic(const FString& Container, const FString& AssignTo)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Ref_AssignTo"), AssignTo);
	FormatArgs.Add(TEXT("Ref_ContainerAddr"), Container);
	FormatArgs.Add(TEXT("Declare_PropertyCPPType"), GetCPPType());
	FormatArgs.Add(TEXT("Num_PropMemOffset"), GetMemOffset());
    FormatArgs.Add(TEXT("Declare_PropertyName"), GetPropertyName());

	return FString::Format(PropDecorator_AssignPropPtrDynamic, FormatArgs);
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
	FormatArgs.Add(TEXT("Declare_PropertyName"), GetPropertyName());
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
		GetCode_AssignPropPointerStatic(
			ContainerName,
			FString::Printf(TEXT("%s* PropAddr"), *GetCPPType())
		)
	);
	FormatArgs.Add(TEXT("Code_BeforeCondition"), ConditionFullStateIsNull ? TEXT("bIsFullStateNull ? true :") : TEXT(""));
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), TEXT("PropAddr"));
	FormatArgs.Add(TEXT("Declare_PropertyName"), GetPropertyName());
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

bool FPropertyDecorator::HasOnRepNotifyParam()
{
	return OriginalProperty->HasAnyPropertyFlags(CPF_RepNotify) && Owner->FindFunctionByName(OriginalProperty->RepNotifyFunc)->NumParms > 0;
}

FString FPropertyDecorator::GetCode_CallRepNotify(const FString& TargetInstanceName, const FString& OldValuePointer)
{
	if (OriginalProperty->HasAnyPropertyFlags(CPF_RepNotify))
	{
		FStringFormatNamedArguments FormatArgs;
		FormatArgs.Add(TEXT("Declare_PropertyName"), GetPropertyName());
		FormatArgs.Add(TEXT("Declare_TargetInstance"), TargetInstanceName);
		FormatArgs.Add(TEXT("Declare_FunctionName"), OriginalProperty->RepNotifyFunc.ToString());
		const UFunction* RepNotifyFunc = Owner->FindFunctionByName(OriginalProperty->RepNotifyFunc);
		FormatArgs.Add(TEXT("Code_OnRepParams"),RepNotifyFunc->NumParms > 0 ? OldValuePointer: TEXT("nullptr"));
		return FString::Format(PropDecorator_CallRepNotifyTemplate, FormatArgs);
	}
	return TEXT("");
}

FString FPropertyDecorator::GetCode_OnStateChange(const FString& TargetInstanceName, const FString& NewStateName, const FString& AfterSetValueCode)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyName"), GetPropertyName());
	FormatArgs.Add(TEXT("Code_HasProtoFieldValue"), GetCode_HasProtoFieldValueIn(NewStateName));
	FormatArgs.Add(TEXT("Code_ActorPropEqualToProtoState"), GetCode_ActorPropEqualToProtoState(TargetInstanceName, NewStateName));
	FormatArgs.Add(
	TEXT("Code_SetPropertyValue"),
		GetCode_SetPropertyValueTo(TargetInstanceName, NewStateName, AfterSetValueCode)
	);
	return FString::Format(PropDecorator_OnChangeStateTemplate, FormatArgs);
}

FString FPropertyDecorator::GetCode_OnStateChangeByMemOffset(const FString& ContainerName, const FString& NewStateName)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(
		TEXT("Code_AssignPropPointers"),
		GetCode_AssignPropPointerStatic(
			ContainerName,
			FString::Printf(TEXT("%s* PropAddr"), *GetCPPType())
		)
	);
	FormatArgs.Add(TEXT("Code_HasProtoFieldValue"), GetCode_HasProtoFieldValueIn(NewStateName));
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), TEXT("PropAddr"));
	FormatArgs.Add(TEXT("Declare_PropertyName"), GetPropertyName());
	FormatArgs.Add(TEXT("Code_GetProtoFieldValue"), GetCode_GetProtoFieldValueFrom(NewStateName));
	return FString::Format(PropDeco_OnChangeStateByMemOffsetTemp, FormatArgs);
}

FString FPropertyDecorator::GetCode_SetPropertyValueArrayInner(const FString& ArrayPropertyName, const FString& PropertyPointer, const FString& NewStateName)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyName"), ArrayPropertyName);
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	return FString::Format(PropDeco_OnChangeStateArrayInnerTemp, FormatArgs);
}

TArray<FString> FPropertyDecorator::GetAdditionalIncludes()
{
	return TArray<FString>();
}

FString FPropertyDecorator::GetCode_GetWorldRef()
{
	return TEXT("");
}

bool FPropertyDecorator::IsStruct()
{
	return false;
}

TArray<TSharedPtr<FStructPropertyDecorator>> FPropertyDecorator::GetStructPropertyDecorators()
{
	return TArray<TSharedPtr<FStructPropertyDecorator>>();
}

FString FPropertyDecorator::GetProtoFieldsDefinition(int *NextIndex)
{
	FString Result = FString::Printf(
		TEXT("%s %s %s = %d;\n"),
		*GetProtoFieldRule(),
		*GetProtoFieldType(),
		*GetProtoFieldName(),
		(*NextIndex)
	);
	(*NextIndex)++;
	return Result;
}
