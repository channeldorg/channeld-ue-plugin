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
	return IsExternallyAccessible() && !Owner->IsBlueprintType();
}

FString FPropertyDecorator::GetPropertyName()
{
	return OriginalProperty->GetName();
}

FString FPropertyDecorator::GetPropertyType()
{
	return TEXT("");
}

FString FPropertyDecorator::GetPointerName()
{
	return GetPropertyName() + TEXT("Ptr");
}

FString FPropertyDecorator::GetCPPType()
{
	return OriginalProperty->GetCPPType();
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
	if(!ForceFromPointer)
	{
		return FString::Printf(TEXT("%s->%s"), *TargetInstance, *GetPropertyName());
	}
	else
	{
		return TEXT("*") + GetPointerName();
	}
}

FString FPropertyDecorator::GetCode_SetPropertyValueTo(const FString& TargetInstance, const FString& NewStataName, const FString& AfterSetValueCode)
{
	return FString::Printf(TEXT("%s = %s;\n%s"), *GetCode_GetPropertyValueFrom(TargetInstance), *GetCode_GetProtoFieldValueFrom(NewStataName), *AfterSetValueCode);
}

FString FPropertyDecorator::GetDeclaration_PropertyPtr()
{
	return FString::Printf(TEXT("%s* %s"), *this->GetCPPType(), *this->GetPointerName());
}

FString FPropertyDecorator::GetCode_AssignPropertyPointer(const FString& Container, const FString& ContainerTemplate, const FString& AssignTo)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Ref_AssignTo"), FStringFormatArg(AssignTo));
	FormatArgs.Add(TEXT("Ref_ContainerAddr"), FStringFormatArg(Container));
	FormatArgs.Add(TEXT("Ref_ContainerTemplate"), FStringFormatArg(ContainerTemplate));
	FormatArgs.Add(TEXT("Declare_PropertyCPPType"), FStringFormatArg(GetCPPType()));
	FormatArgs.Add(TEXT("Declare_PropertyName"), FStringFormatArg(GetPropertyName()));
	
	return FString::Format(PropDecorator_GetCode_AssignPropertyPointerTemplate, FormatArgs);
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

FString FPropertyDecorator::GetCode_SetDeltaState(const FString& TargetInstance, const FString& FullStateName, const FString& DeltaStateName)
{
	FStringFormatNamedArguments FormatArgs; 
	FormatArgs.Add(TEXT("Code_ActorPropEqualToProtoState"), GetCode_ActorPropEqualToProtoState(TargetInstance, FullStateName)); 
	FormatArgs.Add(TEXT("Code_SetProtoFieldValue"), GetCode_SetProtoFieldValueTo(DeltaStateName, GetCode_GetPropertyValueFrom(TargetInstance))); 
	return FString::Format(PropertyDecorator_SetDeltaStateTemplate, FormatArgs); 
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
	return FString::Format(PropertyDecorator_OnChangeStateTemplate, FormatArgs); 
}

TArray<FString> FPropertyDecorator::GetAdditionalIncludes()
{
	return TArray<FString>();
}
