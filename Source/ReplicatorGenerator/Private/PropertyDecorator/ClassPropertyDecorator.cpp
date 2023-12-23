#include "PropertyDecorator/ClassPropertyDecorator.h"

#include "ReplicatorGeneratorManager.h"

FString FClassPropertyDecorator::GetPropertyType()
{
	return TEXT("FClassProperty");
}

FString FClassPropertyDecorator::GetCode_GetProtoFieldValueFrom(const FString& StateName)
{
	return FString::Printf(TEXT("LoadClass<%s>(NULL, UTF8_TO_TCHAR(%s->%s().c_str()))"), *InnerClassName, *StateName, *GetProtoFieldName());
}

FString FClassPropertyDecorator::GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode)
{
	return FString::Printf(TEXT("if (%s)\n%s->set_%s(std::string(TCHAR_TO_UTF8(*(%s)->GetPathName())))"), *GetValueCode, *StateName, *GetProtoFieldName(), *GetValueCode);
}

FString FClassPropertyDecorator::GetCode_SetDeltaStateArrayInner(const FString& PropertyPointer, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	FormatArgs.Add(TEXT("Declare_DeltaStateName"), DeltaStateName);
	FormatArgs.Add(TEXT("Declare_FullStateName"), FullStateName);
	FormatArgs.Add(TEXT("Definition_ProtoName"), GetProtoFieldName());
	return FString::Format(ClassPropDeco_SetDeltaStateArrayInnerTemp, FormatArgs);
}

FString FClassPropertyDecorator::GetCode_SetPropertyValueArrayInner(const FString& ArrayPropertyName, const FString& PropertyPointer, const FString& NewStateName)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_PropertyName"), ArrayPropertyName);
	FormatArgs.Add(TEXT("Declare_PropertyPtr"), PropertyPointer);
	FormatArgs.Add(TEXT("Declare_ClassName"), InnerClassName);
	return FString::Format(ClassPropDeco_OnChangeStateArrayInnerTemp, FormatArgs);
}

TArray<FString> FClassPropertyDecorator::GetAdditionalIncludes()
{
	auto IncludeFiles = FPropertyDecorator::GetAdditionalIncludes();
	if (const auto ModuleInfo = FReplicatorGeneratorManager::Get().GetModuleInfo(InnerClassName))
	{
		IncludeFiles.Add(ModuleInfo->RelativeToModule); 
	}
	return IncludeFiles;
}
