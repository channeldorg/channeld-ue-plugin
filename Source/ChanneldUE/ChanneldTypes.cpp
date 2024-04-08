#include "ChanneldTypes.h"

#include "ChanneldSettings.h"
#include "Net/RepLayout.h"

DEFINE_LOG_CATEGORY(LogChanneld);


class CHANNELDUE_API FChanneldFastArrayNetSerializeCB : public INetSerializeCB
{
public:
	FChanneldFastArrayNetSerializeCB(UNetDriver* InNetDriver)
		: NetDriver(InNetDriver)
	{
	}
	virtual void NetSerializeStruct(FNetDeltaSerializeInfo& Params) override final
	{
		UScriptStruct* Struct = CastChecked<UScriptStruct>(Params.Struct);
		FBitArchive& Ar = Params.Reader ? static_cast<FBitArchive&>(*Params.Reader) : static_cast<FBitArchive&>(*Params.Writer);
		Params.bOutHasMoreUnmapped = false;

		if (EnumHasAnyFlags(Struct->StructFlags, STRUCT_NetSerializeNative))
		{
			UScriptStruct::ICppStructOps* CppStructOps = Struct->GetCppStructOps();
			check(CppStructOps); // else should not have STRUCT_NetSerializeNative
			bool bSuccess = true;
			if (!CppStructOps->NetSerialize(Ar, Params.Map, bSuccess, Params.Data))
			{
				Params.bOutHasMoreUnmapped = true;
			}

			// Check the success of the serialization and print a warning if it failed. This is how native handles failed serialization.
			if (!bSuccess)
			{
				UE_LOG(LogChanneld, Warning, TEXT("FastArrayNetSerialize: NetSerialize %s failed."), *Params.Struct->GetFullName());
			}
		}
		else
		{
			TSharedPtr<FRepLayout> RepLayout = NetDriver->GetStructRepLayout(Params.Struct);
			RepLayout->SerializePropertiesForStruct(Params.Struct, Ar, Params.Map, Params.Data, Params.bOutHasMoreUnmapped, Params.Object);
		}
	}

	void GatherGuidReferencesForFastArray(struct FFastArrayDeltaSerializeParams& Params) override
	{

	}

	bool MoveGuidToUnmappedForFastArray(struct FFastArrayDeltaSerializeParams& Params) override
	{
		return true;
	}

	void UpdateUnmappedGuidsForFastArray(struct FFastArrayDeltaSerializeParams& Params) override
	{
	}

	bool NetDeltaSerializeForFastArray(struct FFastArrayDeltaSerializeParams& Params) override
	{
		return true;
	}

private:
	UNetDriver* NetDriver;
};


bool FChanneldNetDeltaSerializeInfo::DeltaSerializeRead(UNetDriver* NetDriver, FNetBitReader& Reader, UObject* Object, UScriptStruct* NetDeltaStruct, void* Destination)
{
	FChanneldNetDeltaSerializeInfo NetDeltaInfo;

	FChanneldFastArrayNetSerializeCB SerializeCB(NetDriver);

	NetDeltaInfo.Reader = &Reader;
	NetDeltaInfo.Map = Reader.PackageMap;
	NetDeltaInfo.NetSerializeCB = &SerializeCB;
	NetDeltaInfo.Object = Object;

	UScriptStruct::ICppStructOps* CppStructOps = NetDeltaStruct->GetCppStructOps();
	check(CppStructOps);

	return CppStructOps->NetDeltaSerialize(NetDeltaInfo, Destination);
	//return false;
}

bool FChanneldNetDeltaSerializeInfo::DeltaSerializeWrite(UNetDriver* NetDriver, FNetBitWriter& Writer, UObject* Object, UScriptStruct* NetDeltaStruct, void* Source, TSharedPtr<INetDeltaBaseState>& OldState)
{
	FChanneldNetDeltaSerializeInfo NetDeltaInfo;

	FChanneldFastArrayNetSerializeCB SerializeCB(NetDriver);

	NetDeltaInfo.Writer = &Writer;
	NetDeltaInfo.Map = Writer.PackageMap;
	NetDeltaInfo.NetSerializeCB = &SerializeCB;
	NetDeltaInfo.Object = Object;
	NetDeltaInfo.Struct = NetDeltaStruct;
	NetDeltaInfo.bInternalAck = GetMutableDefault<UChanneldSettings>()->bSetInternalAck;

	TSet<FNetworkGUID> LocalReferencedGuids;
	NetDeltaInfo.GatherGuidReferences = &LocalReferencedGuids;

	TSharedPtr<INetDeltaBaseState> NewState;
	NetDeltaInfo.NewState = &NewState;
	NetDeltaInfo.OldState = OldState.Get();

	UScriptStruct::ICppStructOps* CppStructOps = NetDeltaStruct->GetCppStructOps();
	check(CppStructOps);

	if (CppStructOps->NetDeltaSerialize(NetDeltaInfo, Source))
	{
		OldState = std::move(NewState);
		return true;
	}
	return false;
}
