#include "PropertyDecorator/FastArrayPropertyDecorator.h"

FString FFastArrayPropertyDecorator::GetPropertyType()
{
	return TEXT("FStructProperty");
}

FString FFastArrayPropertyDecorator::GetPropertyName()
{
	return FPropertyDecorator::GetPropertyName();
}

FString FFastArrayPropertyDecorator::GetCode_OnStateChange(const FString& TargetInstanceName,
                                                           const FString& NewStateName, const FString& AfterSetValueCode)
{
	const TCHAR* T = LR"EOF(
bool b{Declare_PropertyName}Changed = false;
{
if ({Code_HasProtoFieldValue})
{
	auto StructProperty = CastFieldChecked<const FStructProperty>({Declare_Target}->GetClass()->FindPropertyByName(FName("{Declare_PropertyName}")));
	if (StructProperty)
	{
		if(NewState->{Declare_ProtoFieldName}_full().size() > 0 || NewState->{Declare_ProtoFieldName}().size() > 0)
		{
			UScriptStruct* NetDeltaStruct = Cast<UScriptStruct>(StructProperty->Struct);
			auto NetDriver = {Declare_PropertyOwner}->GetNetDriver();
			UNetConnection* Conn = ChanneldUtils::GetActorNetConnection({Declare_PropertyOwner});
			check(Conn != nullptr);

			if(NewState->{Declare_ProtoFieldName}_full().size() > 0)
			{
				FNetBitReader Reader(Conn->PackageMap, (uint8*)(NewState->{Declare_ProtoFieldName}_full().data()), NewState->{Declare_ProtoFieldName}_full().size() * 8);
				if (FChanneldNetDeltaSerializeInfo::DeltaSerializeRead(NetDriver, Reader, Cast<UObject>({Declare_Target}),
																		   NetDeltaStruct, {Declare_PropertyPointer}))
				{
					UE_LOG(LogTemp, Log, TEXT("DeltaSerializeRead {Declare_PropertyName} full state Success"));
				}

			}
			if(NewState->{Declare_ProtoFieldName}().size() > 0)
			{
				FNetBitReader Reader(Conn->PackageMap, (uint8*)(NewState->{Declare_ProtoFieldName}().data()), NewState->{Declare_ProtoFieldName}().size() * 8);
				if (FChanneldNetDeltaSerializeInfo::DeltaSerializeRead(NetDriver, Reader, Cast<UObject>({Declare_Target}),
																		   NetDeltaStruct, {Declare_PropertyPointer}))
				{
					UE_LOG(LogTemp, Log, TEXT("DeltaSerializeRead {Declare_PropertyName} Success"));
				}
			}

			b{Declare_PropertyName}Changed = true;	//	GetCode_OnStateChange
			{Code_AfterSetValue}


            if (Conn == ChanneldUtils::GetNetConnForSpawn()){
                ChanneldUtils::ResetNetConnForSpawn();
            }
		}
	}
}
bStateChanged |= b{Declare_PropertyName}Changed;
})EOF";

	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Code_HasProtoFieldValue"), GetCode_HasProtoFieldValueIn(NewStateName));
	FormatArgs.Add(TEXT("Declare_ProtoFieldName"), GetProtoFieldName());
	FormatArgs.Add(TEXT("Declare_Target"), TargetInstanceName);
	FormatArgs.Add(TEXT("Declare_PropertyPointer"), GetPointerName());
	FormatArgs.Add(TEXT("Declare_PropertyName"), GetPropertyName());
	FormatArgs.Add(TEXT("Code_AfterSetValue"), AfterSetValueCode);

	if (this->OriginalProperty->Owner.IsUObject() && this->OriginalProperty->Owner.GetOwnerClass()->IsChildOf(
		UActorComponent::StaticClass()))
	{
		FString Declare_Target = FString::Printf(TEXT("%s->GetOwnerActor()"), *TargetInstanceName);
		FormatArgs.Add(TEXT("Declare_PropertyOwner"), Declare_Target);
	}
	else
	{
		FormatArgs.Add(TEXT("Declare_PropertyOwner"), TargetInstanceName);
	}

	return FString::Format(T, FormatArgs);
}

FString FFastArrayPropertyDecorator::GetCode_GetProtoFieldValueFrom(const FString& StateName)
{
	return FString::Printf(TEXT("UTF8_TO_TCHAR(%s->%s().c_str())"), *StateName, *GetProtoFieldName());
}

FString FFastArrayPropertyDecorator::GetCode_SetDeltaState(const FString& TargetInstance, const FString& FullStateName,
                                                           const FString& DeltaStateName, bool ConditionFullStateIsNull)
{
	//*GetCode_GetPropertyValueFrom(TargetInstance);	//Actor->MyFastArrayProperty
	FStringFormatNamedArguments FormatArgs;

	const TCHAR* T = LR"EOF(
bool b{Declare_PropertyName}Changed = false;
{
	auto StructProperty = CastFieldChecked<const FStructProperty>({Declare_Target}->GetClass()->FindPropertyByName(FName("{Declare_PropertyName}")));
	if (StructProperty)
	{
		if ({Declare_PropertyPointer}->ArrayReplicationKey != {Declare_OldStateKey})
		{
			UNetConnection* Conn = ChanneldUtils::GetActorNetConnection({Declare_PropertyOwner});
			check(Conn != nullptr);

			UScriptStruct* NetDeltaStruct = Cast<UScriptStruct>(StructProperty->Struct);
			auto NetDriver = {Declare_PropertyOwner}->GetNetDriver();
            FNetBitWriter ValueDataWriter(Conn->PackageMap, 4096);

			bool doFullUpdate = {Declare_DeltaUpdateCountName} == 0;
			
			TSharedPtr<INetDeltaBaseState> OldState;
			if (!doFullUpdate)
			{
				OldState = {Declare_FullNetStateName};
			}

			if(FChanneldNetDeltaSerializeInfo::DeltaSerializeWrite(
				NetDriver, ValueDataWriter, Cast<UObject>({Declare_Target}), NetDeltaStruct, {Declare_PropertyPointer}, OldState))
			{
				{Declare_DeltaUpdateCountName}++;
				std::string NewData(reinterpret_cast<const char*>(ValueDataWriter.GetData()), ValueDataWriter.GetNumBytes());
				if(doFullUpdate)
				{
                    ActiveGameplayEffectsLastFullNetState = OldState;
					DeltaState->set_{Declare_ProtoFieldName}(std::string());
					DeltaState->set_{Declare_ProtoFieldName}_full(std::move(NewData));
				}
				else
				{
					check(FullState->has_{Declare_ProtoFieldName}_full());
					DeltaState->set_{Declare_ProtoFieldName}(std::move(NewData));
					if(NewData.size() / 2 >= FullState->{Declare_ProtoFieldName}_full().size())
					{
						{Declare_DeltaUpdateCountName} = 0;
					}
				}

				b{Declare_PropertyName}Changed = true;	// GetCode_SetDeltaState
			}

			{Declare_OldStateKey} = {Declare_PropertyPointer}->ArrayReplicationKey ;

            if (Conn == ChanneldUtils::GetNetConnForSpawn()){
                ChanneldUtils::ResetNetConnForSpawn();
            }
		}
	}
	bStateChanged |= b{Declare_PropertyName}Changed;
})EOF";
	
	FormatArgs.Add(TEXT("Declare_OldStateKey"), FString::Printf(TEXT("%sReplicationKey"), *GetPropertyName()));
	FormatArgs.Add(TEXT("Declare_PropertyName"), GetPropertyName());
	FormatArgs.Add(TEXT("Declare_PropertyPointer"), GetPointerName());
	FormatArgs.Add(TEXT("Declare_ProtoFieldName"), GetProtoFieldName());
	FormatArgs.Add(TEXT("Declare_DeltaUpdateCountName"), GetLastFullNetStateName());
	FormatArgs.Add(TEXT("Declare_FullNetStateName"), GetLastFullNetStateName());
	FormatArgs.Add(TEXT("Declare_DeltaUpdateCountName"), GetDeltaUpdateCountName());
	FormatArgs.Add(TEXT("Declare_Target"), TargetInstance);

	if (this->OriginalProperty->Owner.IsUObject() && this->OriginalProperty->Owner.GetOwnerClass()->IsChildOf(
		UActorComponent::StaticClass()))
	{
		FString Declare_Target = FString::Printf(TEXT("%s->GetOwnerActor()"), *TargetInstance);
		FormatArgs.Add(TEXT("Declare_PropertyOwner"), Declare_Target);
	}
	else
	{
		FormatArgs.Add(TEXT("Declare_PropertyOwner"), TargetInstance);
	}
	return FString::Format(T, FormatArgs);
}

FString FFastArrayPropertyDecorator::GetDeclaration_PropertyPtr()
{
	return FString::Printf(
		TEXT("int32 %sReplicationKey = -1; \nTSharedPtr<INetDeltaBaseState> %s; int %s = 0; \n%s"), 
		*GetPropertyName(),
		*GetLastFullNetStateName(),
		*GetDeltaUpdateCountName(),
		*FPropertyDecorator::GetDeclaration_PropertyPtr());
}

FString FFastArrayPropertyDecorator::GetCode_SetProtoFieldValueTo(const FString& StateName, const FString& GetValueCode)
{
	return FString::Printf(
		TEXT("%s->set_%s(std::string(TCHAR_TO_UTF8(*%s)))"), *StateName, *GetProtoFieldName(), *GetValueCode);
}

TArray<FString> FFastArrayPropertyDecorator::GetAdditionalIncludes()
{
	return TArray<FString>{TEXT("ChanneldUtils.h")};
}

FString FFastArrayPropertyDecorator::GetLastFullNetStateName()
{
	return FString::Printf(TEXT("%sLastFullNetState"), *GetPropertyName());
}

FString FFastArrayPropertyDecorator::GetDeltaUpdateCountName()
{
	return FString::Printf(TEXT("%sDeltaUpdateCount"), *GetPropertyName());
}

FString FFastArrayPropertyDecorator::GetFullUpdateProtoFieldName()
{
	return FString::Printf(TEXT("%s_full"), *GetProtoFieldName());
}

FString FFastArrayPropertyDecorator::GetProtoFieldsDefinition(int* NextIndex)
{
	FString Result = FPropertyDecorator::GetProtoFieldsDefinition(NextIndex);
	Result += FString::Printf( TEXT("%s %s %s = %d;\n"),
		*GetProtoFieldRule(), *GetProtoFieldType(), *GetFullUpdateProtoFieldName(), (*NextIndex)
	);
	(*NextIndex)++;
	return Result;
}

FString FFastArrayPropertyDecorator::GetCode_HasProtoFieldValueIn(const FString& StateName)
{
	return FString::Printf(TEXT("%s->%s().size() > 0 || %s->%s().size() > 0"), 
		*StateName, *GetProtoFieldName(),
		*StateName, *GetFullUpdateProtoFieldName());
}
