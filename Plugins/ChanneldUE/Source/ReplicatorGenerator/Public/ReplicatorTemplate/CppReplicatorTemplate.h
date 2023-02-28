#pragma once

static const TCHAR* CodeGen_CPP_HeadCodeTemplate =
	LR"EOF(
#pragma once

#include "CoreMinimal.h"
#include "Replication/ChanneldReplicatorBase.h"
#include "Net/UnrealNetwork.h"
{Code_IncludeActorHeader}
#include "{File_ProtoPbHead}"
{Code_AdditionalInclude}

class {Declare_ReplicatorClassName} : public FChanneldReplicatorBase
{
public:
  {Declare_ReplicatorClassName}(UObject* InTargetObj);
  virtual ~{Declare_ReplicatorClassName}() override;

  //~Begin FChanneldReplicatorBase Interface
{Code_OverrideGetNetGUID}
  virtual UClass* GetTargetClass() override { return {Declare_TargetClassName}::StaticClass(); }
  virtual google::protobuf::Message* GetDeltaState() override;
  virtual void ClearState() override;
  virtual void Tick(float DeltaTime) override;
  virtual void OnStateChanged(const google::protobuf::Message* NewState) override;
  //~End FChanneldReplicatorBase Interface

{Declare_OverrideSerializeAndDeserializeFunctionParams}

protected:
  TWeakObjectPtr<{Declare_TargetClassName}> {Ref_TargetInstanceRef};

  // [Server+Client] The accumulated channel data of the target object
  {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}* FullState;
  // [Server] The accumulated delta change before next send
  {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}* DeltaState;

private:
{Declare_IndirectlyAccessiblePropertyPtrs}

};

namespace {Declare_RPCParamStructNamespace}
{
{Declare_RPCParamStructs}
}
)EOF";

static const TCHAR* CodeGen_SerializeAndDeserializeFunctionParams =
  LR"EOF(
  virtual TSharedPtr<google::protobuf::Message> SerializeFunctionParams(UFunction* Func, void* Params, FOutParmRec* OutParams, bool& bSuccess) override;
  virtual TSharedPtr<void> DeserializeFunctionParams(UFunction* Func, const std::string& ParamsPayload, bool& bSuccess, bool& bDelayRPC) override;
)EOF";

static const TCHAR* CodeGen_CPP_ConstructorImplTemplate =
	LR"EOF(
{Declare_ReplicatorClassName}::{Declare_ReplicatorClassName}(UObject* InTargetObj) : FChanneldReplicatorBase(InTargetObj)
{
  {Ref_TargetInstanceRef} = CastChecked<{Declare_TargetClassName}>(InTargetObj);
  // Remove the registered DOREP() properties in the Actor
  TArray<FLifetimeProperty> RepProps;
  DisableAllReplicatedPropertiesOfClass(InTargetObj->GetClass(), GetTargetClass(), EFieldIteratorFlags::ExcludeSuper, RepProps);

  FullState = new {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName};
  DeltaState = new {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName};

{Code_AssignPropertyPointers}
}
)EOF";

static const TCHAR* CodeGen_CPP_DestructorImplTemplate =
	LR"EOF(
{Declare_ReplicatorClassName}::~{Declare_ReplicatorClassName}()
{
  delete FullState;
  delete DeltaState;
}
)EOF";

static const TCHAR* CodeGen_CPP_GetDeltaStateImplTemplate =
	LR"EOF(
google::protobuf::Message* {Declare_ReplicatorClassName}::GetDeltaState()
{
  return DeltaState;
}
)EOF";

static const TCHAR* CodeGen_CPP_ClearStateImplTemplate =
	LR"EOF(
void {Declare_ReplicatorClassName}::ClearState()
{
  DeltaState->Clear();
  bStateChanged = false;
}
)EOF";

static const TCHAR* CodeGen_CPP_TickImplTemplate =
	LR"EOF(
void {Declare_ReplicatorClassName}::Tick(float DeltaTime)
{
  if (!{Ref_TargetInstanceRef}.IsValid(){Code_TickAdditionalCondition}) { return; }

{Code_AllPropertiesSetDeltaState}

  if (bStateChanged) {
    FullState->MergeFrom(*DeltaState);
  }
}
)EOF";

static const TCHAR* CodeGen_CPP_OnStateChangedImplTemplate =
	LR"EOF(
void {Declare_ReplicatorClassName}::OnStateChanged(const google::protobuf::Message* InNewState)
{
  if (!{Ref_TargetInstanceRef}.IsValid(){Code_OnStateChangedAdditionalCondition}) { return; }

  // Only client needs to apply the new state
  if ({Code_IsClient}) { return; }

  const {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}* NewState = static_cast<const {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}*>(InNewState);
  FullState->MergeFrom(*NewState);
  bStateChanged = false;

  {Code_AllPropertyOnStateChanged}
}
)EOF";

static const TCHAR* CodeGen_CPP_RPCTemplate =
	LR"EOF(
TSharedPtr<google::protobuf::Message> {Declare_ReplicatorClassName}::SerializeFunctionParams(UFunction* Func, void* Params, FOutParmRec* OutParams, bool& bSuccess)
{
	bSuccess = true;
{Code_SerializeFunctionParams}
	bSuccess = false;
	return nullptr;
}

TSharedPtr<void> {Declare_ReplicatorClassName}::DeserializeFunctionParams(UFunction* Func, const std::string& ParamsPayload, bool& bSuccess, bool& bDelayRPC)
{
	bSuccess = true;
{Code_DeserializeFunctionParams}
	bSuccess = false;
	return nullptr;
}
)EOF";

static const TCHAR* CodeGen_ProtoTemplate =
	LR"EOF(
syntax = "proto3";
{Code_Import}
{Option}
package {Declare_ProtoPackageName};
{Definition_ProtoStateMsg}
)EOF";

static const TCHAR* CodeGen_ProtoStateMessageTemplate =
  LR"EOF(
message {Declare_StateMessageType} {
{Declare_ProtoFields}

{Declare_SubProtoFields}
}
)EOF";

static const FString CodeGen_RegisterReplicatorTemplate =
	FString(LR"EOF(
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Replication/ChanneldReplication.h"
{Code_IncludeActorHeaders}
#include "ChanneldReplicatorRegistration.generated.h"

)EOF")
	+ TEXT("UCLASS()")
	+ LR"EOF(
class UChanneldReplicatorRegistration : public UEngineSubsystem
{
  GENERATED_BODY()
  virtual void Initialize(FSubsystemCollectionBase& Collection) override
  {
{Code_ReplicatorRegister}
{Code_ChannelDataProcessorRegister}
  }
  virtual void Deinitialize() override
  {
{Code_ChannelDataProcessorUnregister}
  }

private:
  {Declaration_Variables}
};
)EOF";

static const FString CodeGen_SerializeFuncParamTemp =
  LR"EOF(
TSharedPtr<google::protobuf::Message> {Declare_ReplicatorClassName}::SerializeFunctionParams(UFunction* Func, void* Params, FOutParmRec* OutParams, bool& bSuccess)
{
	bSuccess = true;
{Code_SerializeFuncParam}
	bSuccess = false;
	return nullptr;
}
)EOF";

static const FString CodeGen_ChannelDataProcessorCPPTemp =
  LR"EOF(
#pragma once
#include "ChannelDataInterfaces.h"

#include "{File_CDP_ProtoHeader}"
#include "unreal_common.pb.h"
#include "ChanneldUE/ChanneldTypes.h"
#include "Components/ActorComponent.h"
#include "Replication/ChanneldReplication.h"
{Code_IncludeAdditionHeaders}

namespace {Declaration_CDP_Namespace}
{
  class {Declaration_CDP_ClassName} : public IChannelDataProcessor
  {
  protected:
  	TUniquePtr<unrealpb::ActorState> RemovedActorState;
  	TUniquePtr<unrealpb::ActorComponentState> RemovedActorComponentState;
  
  public:

    {Code_ConstClassPathFNameVariable}
    
    {Declaration_CDP_ClassName}()
    {
      RemovedActorState = MakeUnique<{Definition_ChanneldUEBuildInProtoNamespace}::ActorState>();
      RemovedActorState->set_removed(true);
      RemovedActorComponentState = MakeUnique<{Definition_ChanneldUEBuildInProtoNamespace}::ActorComponentState>();
      RemovedActorComponentState->set_removed(true);
    }
    virtual bool Merge(const google::protobuf::Message* SrcMsg, google::protobuf::Message* DstMsg) override
    {
      auto Src = static_cast<const {Definition_CDP_ProtoNamespace}::{Definition_CDP_ProtoMsgName}*>(SrcMsg);
      auto Dst = static_cast<{Definition_CDP_ProtoNamespace}::{Definition_CDP_ProtoMsgName}*>(DstMsg);
    {Code_Merge}
      return true;
    }
    
    virtual const google::protobuf::Message* GetStateFromChannelData(google::protobuf::Message* ChannelData, UClass* TargetClass, uint32 NetGUID, bool& bIsRemoved) override
    {
      if(ChannelData == nullptr) {
        UE_LOG(LogChanneld, Error, TEXT("ChannelData is nullptr"));
        bIsRemoved = false;
        return nullptr;
      }
      auto {Declaration_CDP_ProtoVar} = static_cast<{Definition_CDP_ProtoNamespace}::{Definition_CDP_ProtoMsgName}*>(ChannelData);
      {Code_GetStateFromChannelData}
      else
      {
        UE_LOG(LogChanneld, Warning, TEXT("State of '%s' is not supported in the ChannelData, NetGUID: %d"), *TargetClass->GetName(), NetGUID);
      }
    
      bIsRemoved = false;
      return nullptr;
    }
    
    virtual void SetStateToChannelData(const google::protobuf::Message* State, google::protobuf::Message* ChannelData, UClass* TargetClass, uint32 NetGUID) override
    {
      if(ChannelData == nullptr) {
        UE_LOG(LogChanneld, Error, TEXT("ChannelData is nullptr"));
        return;
      }
      auto {Declaration_CDP_ProtoVar} = static_cast<{Definition_CDP_ProtoNamespace}::{Definition_CDP_ProtoMsgName}*>(ChannelData);
      {Code_SetStateToChannelData}
      else
      {
        UE_LOG(LogChanneld, Warning, TEXT("State of '%s' is not supported in the ChannelData, NetGUID: %d"), *TargetClass->GetName(), NetGUID);
      }
    }
  };
}
)EOF";
