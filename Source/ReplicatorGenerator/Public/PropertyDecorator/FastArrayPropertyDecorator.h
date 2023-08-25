#pragma once
#include "PropertyDecorator.h"

const static TCHAR* FastArrayPropDeco_SetDeltaStateArrayInnerTemp =
	LR"EOF(
FStructProperty* StructProperty = CastField<FStructProperty>(Property);
if (StructProperty)
{
	UScriptStruct* NetDeltaStruct = Cast<UScriptStruct>(StructProperty->Struct);
	auto NetDriver = ActorComp->GetOwnerActor()->GetNetDriver();
	UNetConnection* conn = ActorComp->GetOwnerActor()->GetNetConnection();
	if (!conn) // AI set a default connection
		{
		conn = GWorld->NetDriver->ClientConnections.Top();
		}

	check(conn != nullptr);
	FNetBitWriter ValueDataWriter(conn->PackageMap, 4095);

	if (!FChanneldNetDeltaSerializeInfo::DeltaSerializeWrite(
		NetDriver, ValueDataWriter, ActorComp.Get(), NetDeltaStruct, ActiveGameplayEffects_Ptr,
		ActiveGameplayEffects_OldState))
	{
		continue;
	}

	std::string NewData(reinterpret_cast<const char*>(ValueDataWriter.GetData()),
						ValueDataWriter.GetNumBytes());
	if (!(NewData == FullState->activegameplayeffects()))
	{
		DeltaState->set_activegameplayeffects(NewData);
		bStateChanged = true;
	
)EOF";

const static TCHAR* FastArrayPropDeco_OnChangeStateArrayInnerTemp =
	LR"EOF(
FString NewString = UTF8_TO_TCHAR(MessageArr[i].c_str());
if ((*{Declare_PropertyPtr})[i] != NewString)
{
  (*{Declare_PropertyPtr})[i] = NewString;
  if (!bPropChanged)
  {
    bPropChanged = true;
  }
}
)EOF";

class FFastArrayPropertyDecorator : public FPropertyDecorator
{
public:
	FFastArrayPropertyDecorator(FProperty* InProperty, IPropertyDecoratorOwner* InOwner)
		: FPropertyDecorator(InProperty, InOwner)
	{
		ProtoFieldType = TEXT("bytes");
		SetForceNotDirectlyAccessible(true);
	}
	virtual FString GetPropertyType() override;
	virtual FString GetPropertyName();

	virtual FString GetCode_OnStateChange(const FString& TargetInstanceName, const FString& NewStateName, bool NeedCallRepNotify) override;
	virtual FString GetCode_GetProtoFieldValueFrom(const FString& StateName) override;
	FString GetCode_SetDeltaState(const FString& TargetInstance, const FString& FullStateName, const FString& DeltaStateName, bool ConditionFullStateIsNull) override;
	virtual FString GetDeclaration_PropertyPtr() override;
	virtual FString GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode) override;
	virtual TArray<FString> GetAdditionalIncludes() override;
	virtual FString GetCode_HasProtoFieldValueIn(const FString& StateName) override;

	FString GetLastFullNetStateName();
	FString GetDeltaUpdateCountName();
	FString GetFullUpdateProtoFieldName();

	FString GetProtoFieldsDefinition(int* NextIndex) override;

};
