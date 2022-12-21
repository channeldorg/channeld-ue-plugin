#pragma once

static const TCHAR* CodeGen_BP_HeadCodeTemplate =
	LR"EOF(
#pragma once

#include "CoreMinimal.h"
#include "Replication/ChanneldReplicatorBase.h"
#include "Net/UnrealNetwork.h"
#include "{File_ProtoPbHead}"
{Code_AdditionalInclude}

class {Declare_ReplicatorClassName} : public FChanneldReplicatorBase_BP
{
public:
  {Declare_ReplicatorClassName}(UObject* InTargetObj, const FString& BlueprintPath);
  virtual ~{Declare_ReplicatorClassName}() override;

  //~Begin FChanneldReplicatorBase Interface
  virtual google::protobuf::Message* GetDeltaState() override;
  virtual void ClearState() override;
  virtual void Tick(float DeltaTime) override;
  virtual void OnStateChanged(const google::protobuf::Message* NewState) override;
  //~End FChanneldReplicatorBase Interface

{Declare_OverrideSerializeAndDeserializeFunctionParams}

protected:
  TWeakObjectPtr<{Declare_TargetBaseClassName}> {Ref_TargetInstanceRef};

  // [Server+Client] The accumulated channel data of the target object
  {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}* FullState;
  // [Server] The accumulated delta change before next send
  {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}* DeltaState;

private:
{Declare_IndirectlyAccessiblePropertyPtrs}

};

{Declare_RPCParamStructs}
)EOF";

static const TCHAR* CodeGen_BP_ConstructorImplTemplate =
  LR"EOF(
{Declare_ReplicatorClassName}::{Declare_ReplicatorClassName}(UObject* InTargetObj, const FString& BlueprintPath)
	: FChanneldReplicatorBase_BP(InTargetObj, BlueprintPath)
{
  {Ref_TargetInstanceRef} = CastChecked<{Declare_TargetBaseClassName}>(InTargetObj);
  // Remove the registered DOREP() properties in the Actor
  TArray<FLifetimeProperty> RepProps;
  DisableAllReplicatedPropertiesOfClass(InTargetObj->GetClass(), GetTargetClass(), EFieldIteratorFlags::ExcludeSuper, RepProps);

  FullState = new {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName};
  DeltaState = new {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName};

{Code_AssignPropertyPointers}
}
)EOF";
