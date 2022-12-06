#include "PropertyDecorator/StructPropertyDecorator.h"

#include "PropertyDecoratorFactory.h"
#include "ReplicatorGeneratorDefinition.h"
#include "ReplicatorTemplate/CppActorTemplate.h"

FStructPropertyDecorator::FStructPropertyDecorator(FProperty* InProperty, IPropertyDecoratorOwner* InOwner) : FPropertyDecorator(InProperty, InOwner)
{
	// ProtoFieldType = TEXT("message");
	// InProperty->HasAnyFlags()
	FPropertyDecoratorFactory PropertyDecoratorFactory = FPropertyDecoratorFactory::Get();
	const FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(OriginalProperty);
	const UScriptStruct* ScriptStruct = StructProperty->Struct;
	for (TFieldIterator<FProperty> SIt(ScriptStruct); SIt; ++SIt)
	{
		TSharedPtr<FPropertyDecorator> PropertyDecoratorPtr = PropertyDecoratorFactory.GetPropertyDecorator(*SIt, this);
		if (PropertyDecoratorPtr.IsValid())
		{
			Properties.Add(PropertyDecoratorPtr);
		}
	}
}

bool FStructPropertyDecorator::Init()
{
	for (TSharedPtr<FPropertyDecorator> PropertyDecorator : Properties)
	{
		PropertyDecorator->Init();
	}
	return true;
}

bool FStructPropertyDecorator::IsBlueprintType()
{
	return true;
}

FString FStructPropertyDecorator::GetProtoFieldType()
{
	return FString::Printf(TEXT("%s.%s"), *GenManager_GlobalStructProtoPackage, *GetProtoStateMessageType());
}

FString FStructPropertyDecorator::GetProtoPackageName()
{
	return GenManager_GlobalStructProtoNamespace;
}

FString FStructPropertyDecorator::GetProtoNamespace()
{
	return GenManager_GlobalStructProtoNamespace;
}

FString FStructPropertyDecorator::GetProtoStateMessageType()
{
	return GetCPPType() + TEXT("State");
}

FString FStructPropertyDecorator::GetPropertyType()
{
	return TEXT("FStructProperty");
}

FString FStructPropertyDecorator::GetDeclaration_PropertyPtr()
{
	return FString::Printf(TEXT("%s %s"), *GetDeclaration_PropPtrGroupStructName(), *GetPointerName());
}

FString FStructPropertyDecorator::GetCode_AssignPropertyPointer(const FString& Container, const FString& ContainerTemplate, const FString& AssignTo)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Ref_AssignTo"), FStringFormatArg(AssignTo));
	FormatArgs.Add(TEXT("Ref_ContainerAddr"), FStringFormatArg(Container));
	FormatArgs.Add(TEXT("Ref_ContainerTemplate"), FStringFormatArg(ContainerTemplate));
	FormatArgs.Add(TEXT("Declare_PropertyCPPType"), FStringFormatArg(GetCPPType()));
	FormatArgs.Add(TEXT("Declare_PropertyName"), FStringFormatArg(GetPropertyName()));
	FormatArgs.Add(TEXT("Declare_PropPtrGroupStructName"), FStringFormatArg(GetDeclaration_PropPtrGroupStructName()));

	return FString::Format(StructPropDeco_AssignPropertyPointerTemp, FormatArgs);
}

TArray<FString> FStructPropertyDecorator::GetAdditionalIncludes()
{
	return TArray<FString>{GenManager_GlobalStructHeaderFile, GenManager_GlobalStructProtoHeaderFile};
}

FString FStructPropertyDecorator::GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState)
{
	// return FString::Printf(TEXT("%s.IsSameAs(&%s)"), *GetPointerName(), *GetCode_GetProtoFieldValueFrom(FromState));
	return TEXT("false");
}

FString FStructPropertyDecorator::GetCode_ActorPropEqualToProtoState(const FString& FromActor, const FString& FromState, bool ForceFromPointer)
{
	return GetCode_ActorPropEqualToProtoState(FromActor, FromState);
}

FString FStructPropertyDecorator::GetCode_SetDeltaState(const FString& TargetInstance, const FString& FullStateName, const FString& DeltaStateName)
{
	return FString::Printf(TEXT("if (%s.Merge(&%s->%s(), %s->mutable_%s()))\n{\n  bStateChanged = true;\n}\n"), *GetPointerName(), *FullStateName, *GetProtoFieldName(), *DeltaStateName, *GetProtoFieldName());
}

FString FStructPropertyDecorator::GetCode_SetPropertyValueTo(const FString& TargetInstance, const FString& NewStateName, const FString& AfterSetValueCode)
{
	return FString::Printf(TEXT("if (%s.SetPropertyValue(&%s->%s()))\n{\nbStateChanged = true;\n%s\n}\n"), *GetPointerName(), *NewStateName, *GetProtoFieldName(), *AfterSetValueCode);
}

FString FStructPropertyDecorator::GetDeclaration_PropPtrGroupStruct()
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropPtrGroupStructName"), FStringFormatArg(GetDeclaration_PropPtrGroupStructName()));
	FString Code_AssignPropertyPointers;
	FString AssignPropPtrCodes;
	FString DeclarePropPtrCodes;
	FString PointerValueEqualToStateValueCodes;
	FString SetDeltaStateCodes;
	FString OnStateChangeCodes;
	bool bFirstProperty = true;
	for (TSharedPtr<FPropertyDecorator> PropDecorator : Properties)
	{
		DeclarePropPtrCodes.Append(FString::Printf(TEXT("%s;\n"), *PropDecorator->GetDeclaration_PropertyPtr()));
		AssignPropPtrCodes.Append(
			FString::Printf(
				TEXT("{\n%s;\n}\n"),
				*PropDecorator->GetCode_AssignPropertyPointer(TEXT("Container"), TEXT("ContainerTemplate"), PropDecorator->GetPointerName())
			)
		);
		if (!bFirstProperty) { PointerValueEqualToStateValueCodes.Append(TEXT(" && ")); }
		PointerValueEqualToStateValueCodes.Append(*PropDecorator->GetCode_ActorPropEqualToProtoState(TEXT("this"),TEXT("InState"), true));
		SetDeltaStateCodes.Append(PropDecorator->GetCode_SetDeltaState(TEXT("this"), TEXT("FullState"), TEXT("DeltaState")));
		OnStateChangeCodes.Append(PropDecorator->GetCode_OnStateChange(TEXT("this"), TEXT("NewState")));
		if (bFirstProperty) { bFirstProperty = false; }
	}
	FormatArgs.Add(TEXT("Code_AssignPropertyPointers"), *AssignPropPtrCodes);
	FormatArgs.Add(TEXT("Declare_PropertyPointers"), *DeclarePropPtrCodes);
	FormatArgs.Add(TEXT("Declare_ProtoNamespace"), GetProtoNamespace());
	FormatArgs.Add(TEXT("Declare_ProtoStateMsgName"), GetProtoStateMessageType());
	FormatArgs.Add(TEXT("Code_PointerValuesEqualToStateValues"), PointerValueEqualToStateValueCodes);
	FormatArgs.Add(TEXT("Code_SetDeltaStates"), SetDeltaStateCodes);
	FormatArgs.Add(TEXT("Code_OnStateChange"), OnStateChangeCodes);


	return FString::Format(StructPropDeco_PropPtrGroupStructTemp, FormatArgs);
}

FString FStructPropertyDecorator::GetDeclaration_PropPtrGroupStructName()
{
	return FString::Printf(TEXT("F%sPropPtrGroup"), *GetCPPType());
}

FString FStructPropertyDecorator::GetDefinition_ProtoStateMessage()
{
	FString FieldDefinitions;
	for (int32 i = 0; i < Properties.Num(); i++)
	{
		const TSharedPtr<FPropertyDecorator> Property = Properties[i];
		FieldDefinitions += Property->GetDefinition_ProtoField(i + 1) + TEXT(";\n");
	}
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_StateMessageType"), GetProtoStateMessageType());
	FormatArgs.Add(TEXT("Declare_ProtoFields"), FieldDefinitions);

	return FString::Format(CodeGen_ProtoStateMessageTemplate, FormatArgs);
}
