#include "ChanneldReplicationDriver.h"
#include "ChanneldReplicationComponent.h"
#include "Net/RepLayout.h"
#include "ChanneldConnection.h"
#include "Misc/NetworkGuid.h"
#include "ChanneldUtils.h"

UChanneldReplicationDriver::UChanneldReplicationDriver()
{

}
/*
bool UChanneldReplicationDriver::ProcessRemoteFunction(class AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, UObject* SubObject)
{
	UE_LOG(LogChanneld, Verbose, TEXT("Process %s::%s, SubObject: %s"), *Actor->GetName(), *Function->GetName(), *GetNameSafe(SubObject));
	//return false;

	auto RepComp = Cast<UChanneldReplicationComponent>(Actor->FindComponentByClass(UChanneldReplicationComponent::StaticClass()));
	if (RepComp)
	{
		auto ParamsMsg = RepComp->SerializeFunctionParams(Actor, Function, Parameters);
		if (ParamsMsg)
		{
			auto ChanneldConn = GEngine->GetEngineSubsystem<UChanneldConnection>();
			unrealpb::RemoteFunctionMessage RpcMsg;
			RpcMsg.mutable_targetobj()->MergeFrom(ChanneldUtils::GetRefOfObject(Actor));
			RpcMsg.set_functionname(TCHAR_TO_UTF8(*Function->GetName()));
			RpcMsg.set_paramspayload(ParamsMsg->SerializeAsString());
			ChanneldConn->Send(0, MessageType_RPC, RpcMsg);

			return true;
		}
	}
	return false;
	//return RepComp != nullptr;

	auto Connection = Actor->GetNetConnection();
	if (!Connection)
	{
		return false;
	}

	// -----------------------------
	// UReplicationGraph::ProcessRemoteFunction - Setup
	// -----------------------------
	
	// get the top most function
	while (Function->GetSuperFunction())
	{
		Function = Function->GetSuperFunction();
	}

	// If we have a subobject, thats who we are actually calling this on. If no subobject, we are calling on the actor.
	UObject* TargetObj = SubObject ? SubObject : Actor;

	// Make sure this function exists for both parties.
	const FClassNetCache* ClassCache = NetDriver->NetCache->GetClassNetCache(TargetObj->GetClass());
	if (!ClassCache)
	{
		UE_LOG(LogReplicationGraph, Warning, TEXT("ClassNetCache empty, not calling %s::%s"), *Actor->GetName(), *Function->GetName());
		return true;
	}

	const FFieldNetCache* FieldCache = ClassCache->GetFromField(Function);
	if (!FieldCache)
	{
		UE_LOG(LogReplicationGraph, Warning, TEXT("FieldCache empty, not calling %s::%s"), *Actor->GetName(), *Function->GetName());
		return true;
	}

	// -----------------------------
	// UReplicationGraph::ProcessRemoteFunction - Single Connection
	// -----------------------------

	// Route RPC calls to actual connection
	if (Connection->GetUChildConnection())
	{
		Connection = ((UChildConnection*)Connection)->Parent;
	}

	UActorChannel* Ch = FindOrCreateChannel(Actor, Connection);
	if (!Ch)
	{
		UE_LOG(LogNet, Warning, TEXT("Failed to find or create actor channel, not calling %s::%s"), *Actor->GetName(), *Function->GetName());
		return false;
	}

	// -----------------------------
	// UNetDriver::InternalProcessRemoteFunctionPrivate
	// -----------------------------

	// Use the replication layout to send the rpc parameter values
	TSharedPtr<FRepLayout> RepLayout = Connection->Driver->GetFunctionRepLayout(Function);
	FNetBitWriter TempWriter(Connection->PackageMap, 0);
	RepLayout->SendPropertiesForRPC(Function, Ch, TempWriter, Parameters);
	if (TempWriter.IsError())
	{
		UE_LOG(LogNet, Warning, TEXT("Error: Can't send function '%s' on '%s': Failed to serialize properties"), *GetNameSafe(Function), *GetFullNameSafe(Actor));
		return false;
	}

	FNetFieldExportGroup* NetFieldExportGroup = Ch->GetOrCreateNetFieldExportGroupForClassNetCache(TargetObj);
	FNetBitWriter TempBlockWriter(Connection->PackageMap, 0);
	Ch->WriteFieldHeaderAndPayload(TempBlockWriter, ClassCache, FieldCache, NetFieldExportGroup, TempWriter);
	
	FNetBitWriter PayloadWriter;
	int32 ParameterBits = TempBlockWriter.GetNumBits();
	int32 HeaderBits = Ch->WriteContentBlockPayload(TargetObj, PayloadWriter, false, TempBlockWriter);
	if (PayloadWriter.IsError())
	{
		UE_LOG(LogNet, Log, TEXT("Error: Can't send function '%s' on '%s': RPC bunch overflowed (too much data in parameters?)"), *GetNameSafe(Function), *GetFullNameSafe(TargetObj));
		ensureMsgf(false, TEXT("Error: Can't send function '%s' on '%s': RPC bunch overflowed (too much data in parameters?)"), *GetNameSafe(Function), *GetFullNameSafe(TargetObj));
	}

	check(PayloadWriter.GetNumBits() == HeaderBits + ParameterBits);

	// Send to channeld's GLOBAL channel
	auto ChanneldConn = GEngine->GetEngineSubsystem<UChanneldConnection>();
	ChanneldConn->SendRaw(0, MessageType_RPC, PayloadWriter.GetData(), PayloadWriter.GetNumBytes());

	return true;
}
*/

UActorChannel* UChanneldReplicationDriver::FindOrCreateChannel(AActor* Actor, UNetConnection* Connection)
{
	UActorChannel* Ch = Connection->FindActorChannelRef(Actor);
	if (Ch == nullptr)
	{
		if (Actor->IsPendingKillPending() || !Connection->Driver->IsLevelInitializedForActor(Actor, Connection))
		{
			// We can't open a channel for this actor here
			return nullptr;
		}

		Ch = (UActorChannel*)Connection->CreateChannelByName(NAME_Actor, EChannelCreateFlags::OpenedLocally);
		Ch->SetChannelActor(Actor, ESetChannelActorFlags::None);
	}
	return Ch;
}

bool UChanneldReplicationDriver::ReceivedRPC(AActor* Actor, const FName& FunctionName, const std::string& ParamsPayload, TSet<FNetworkGUID>& UnmappedGuids)
{
	UFunction* Function = Actor->FindFunction(FunctionName);

	//// Make sure this function exists for both parties.
	//const FClassNetCache* ClassCache = NetDriver->NetCache->GetClassNetCache(Actor->GetClass());
	//if (!ClassCache)
	//{
	//	UE_LOG(LogReplicationGraph, Warning, TEXT("ClassNetCache empty, not calling %s::%s"), *Actor->GetName(), *Function->GetName());
	//	return true;
	//}

	//const FFieldNetCache* FieldCache = ClassCache->GetFromField(Function);
	//if (!FieldCache)
	//{
	//	UE_LOG(LogReplicationGraph, Warning, TEXT("FieldCache empty, not calling %s::%s"), *Actor->GetName(), *Function->GetName());
	//	return true;
	//}

	// -----------------------------
	// FObjectReplicator::ReceivedRPC
	// -----------------------------
	uint8* Parms = new(FMemStack::Get(), MEM_Zeroed, Function->ParmsSize)uint8;

	// Use the replication layout to receive the rpc parameter values
	UFunction* LayoutFunction = Function;
	while (LayoutFunction->GetSuperFunction())
	{
		LayoutFunction = LayoutFunction->GetSuperFunction();
	}

	UNetConnection* Connection = Actor->GetNetConnection();
	TSharedPtr<FRepLayout> FuncRepLayout = Connection->Driver->GetFunctionRepLayout(LayoutFunction);
	if (!FuncRepLayout.IsValid())
	{
		UE_LOG(LogRep, Error, TEXT("ReceivedRPC: GetFunctionRepLayout returned an invalid layout."));
		return false;
	}

	uint8* Data = reinterpret_cast<uint8*>(const_cast<char*>(ParamsPayload.data()));
	FNetBitReader Reader(Connection->PackageMap, Data, ParamsPayload.size() * 8);
	FRepObjectDataBuffer Buffer = Parms;
	//FuncRepLayout->ReceivePropertiesForRPC(Actor, LayoutFunction, FindOrCreateChannel(Actor, Connection), Reader, Buffer, UnmappedGuids);

	if (Reader.IsError())
	{
		UE_LOG(LogRep, Error, TEXT("ReceivedRPC: ReceivePropertiesForRPC - Reader.IsError() == true: Function: %s, Object: %s"), *FunctionName.ToString(), *Actor->GetFullName());
		return false;
	}

	if (Reader.GetBitsLeft() != 0)
	{
		UE_LOG(LogNet, Error, TEXT("ReceivedRPC: ReceivePropertiesForRPC - Mismatch read. Function: %s, Object: %s"), *FunctionName.ToString(), *Actor->GetFullName());
		return false;
	}

	// Reset errors from replay driver
	RPC_ResetLastFailedReason();

	/* TODO: Handle unmapped GUIDs?
	if (UnmappedGuids.Num() > 0)
	{
		PendingLocalRPCs.Emplace(FieldCache, RepFlags, Reader, UnmappedGuids);
	}
	*/

	// Call the function.
	Actor->ProcessEvent(Function, Parms);

	return true;
}

//void UChanneldReplicationDriver::SetRepDriverWorld(UWorld* InWorld)
//{
//
//}
//
//void UChanneldReplicationDriver::InitForNetDriver(UNetDriver* InNetDriver)
//{
//
//}
//
//void UChanneldReplicationDriver::InitializeActorsInWorld(UWorld* InWorld)
//{
//
//}
//
//void UChanneldReplicationDriver::ResetGameWorldState()
//{
//
//}
//
//void UChanneldReplicationDriver::AddClientConnection(UNetConnection* NetConnection)
//{
//
//}
//
//void UChanneldReplicationDriver::RemoveClientConnection(UNetConnection* NetConnection)
//{
//
//}
//
//void UChanneldReplicationDriver::AddNetworkActor(AActor* Actor)
//{
//
//}
//
//void UChanneldReplicationDriver::RemoveNetworkActor(AActor* Actor)
//{
//
//}
//
//void UChanneldReplicationDriver::ForceNetUpdate(AActor* Actor)
//{
//
//}
//
//void UChanneldReplicationDriver::FlushNetDormancy(AActor* Actor, bool WasDormInitial)
//{
//
//}
//
//void UChanneldReplicationDriver::NotifyActorTearOff(AActor* Actor)
//{
//
//}
//
//void UChanneldReplicationDriver::NotifyActorFullyDormantForConnection(AActor* Actor, UNetConnection* Connection)
//{
//
//}
//
//void UChanneldReplicationDriver::NotifyActorDormancyChange(AActor* Actor, ENetDormancy OldDormancyState)
//{
//
//}
//
//void UChanneldReplicationDriver::NotifyDestructionInfoCreated(AActor* Actor, FActorDestructionInfo& DestructionInfo)
//{
//
//}
//
//void UChanneldReplicationDriver::SetRoleSwapOnReplicate(AActor* Actor, bool bSwapRoles)
//{
//
//}
//
//int32 UChanneldReplicationDriver::ServerReplicateActors(float DeltaSeconds)
//{
//	return 0;
//}
