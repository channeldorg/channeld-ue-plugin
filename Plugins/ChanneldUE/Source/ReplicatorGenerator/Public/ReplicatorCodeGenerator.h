#pragma once
#include "ReplicatedActorDecorator.h"

static const FString CodeGen_HeadFileExtension = TEXT(".h");
static const FString CodeGen_CppFileExtension = TEXT(".cpp");
static const FString CodeGen_ProtoFileExtension = TEXT(".proto");
static const FString CodeGen_ProtoPbHeadExtension = TEXT(".pb.h");

static const TCHAR* CodeGen_HeadCodeTemplate =
	LR"EOF(
#pragma once

#include "CoreMinimal.h"
#include "Replication/ChanneldReplicatorBase.h"
#include "Net/UnrealNetwork.h"
#include "{File_ActorHeaderFile}"
#include "{File_ProtoPbHead}"

class {Declare_ReplicatorClassName} : public FChanneldReplicatorBase
{
public:
  {Declare_ReplicatorClassName}(UObject* InTargetObj);
  virtual ~{Declare_ReplicatorClassName}() override;

  //~Begin FChanneldReplicatorBase Interface
  virtual UClass* GetTargetClass() override { return {Declare_TargetClassName}::StaticClass(); }
  virtual google::protobuf::Message* GetDeltaState() override;
  virtual void ClearState() override;
  virtual void Tick(float DeltaTime) override;
  virtual void OnStateChanged(const google::protobuf::Message* NewState) override;
  //~End FChanneldReplicatorBase Interface

protected:
  TWeakObjectPtr<{Declare_TargetClassName}> {Ref_TargetInstanceRef};

  // [Server+Client] The accumulated channel data of the target object
  {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}* FullState;
  // [Server] The accumulated delta change before next send
  {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}* DeltaState;
};
	)EOF";

static const TCHAR* CodeGen_ConstructorImplTemplate =
	LR"EOF(
{Declare_ReplicatorClassName}::{Declare_ReplicatorClassName}(UObject* InTargetObj) : FChanneldReplicatorBase(InTargetObj)
{
  {Ref_TargetInstanceRef} = CastChecked<{Declare_TargetClassName}>(InTargetObj);
  // Remove the registered DOREP() properties in the Actor
  TArray<FLifetimeProperty> RepProps;
  DisableAllReplicatedPropertiesOfClass(InTargetObj->GetClass(), {Declare_TargetClassName}::StaticClass(), EFieldIteratorFlags::ExcludeSuper, RepProps);

  FullState = new {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName};
  DeltaState = new {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName};
}
	)EOF";

static const TCHAR* CodeGen_DestructorImplTemplate =
	LR"EOF(
{Declare_ReplicatorClassName}::~{Declare_ReplicatorClassName}()
{
  delete FullState;
  delete DeltaState;
}
	)EOF";

static const TCHAR* CodeGen_GetDeltaStateImplTemplate =
	LR"EOF(
google::protobuf::Message* {Declare_ReplicatorClassName}::GetDeltaState()
{
  return DeltaState;
}
	)EOF";

static const TCHAR* CodeGen_ClearStateImplTemplate =
	LR"EOF(
void {Declare_ReplicatorClassName}::ClearState()
{
  DeltaState->Clear();
  bStateChanged = false;
}
	)EOF";

static const TCHAR* CodeGen_TickImplTemplate =
	LR"EOF(
void {Declare_ReplicatorClassName}::Tick(float DeltaTime)
{
  if (!{Ref_TargetInstanceRef}.IsValid()) { return; }

  {Code_AllPropertiesSetDeltaState}

  FullState->MergeFrom(*DeltaState);
}
	)EOF";

static const TCHAR* CodeGen_OnStateChangedImplTemplate =
	LR"EOF(
void {Declare_ReplicatorClassName}::OnStateChanged(const google::protobuf::Message* InNewState)
{
  if (!{Ref_TargetInstanceRef}.IsValid())
  {
    return;
  }

  // Only client needs to apply the new state
  if ({Ref_TargetInstanceRef}->HasAuthority())
  {
  	return;
  }

  const {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}* NewState = static_cast<const {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}*>(InNewState);
  FullState->MergeFrom(*NewState);
  bStateChanged = false;

  {Code_AllPropertyOnStateChanged}
}
	)EOF";

static const TCHAR* CodeGen_ProtoTemplate =
	LR"EOF(
syntax = "proto3";
package {Declare_ProtoPackageName};
{Definition_ProtoStateMsg}
	)EOF";

struct FReplicatorCodeGroup
{
	TSharedPtr<FReplicatedActorDecorator> Target;

	FString HeadFileName;
	FString HeadCode;

	FString CppFileName;
	FString CppCode;

	FString ProtoFileName;
	FString ProtoDefinitions;
};

class REPLICATORGENERATOR_API FReplicatorCodeGenerator
{
public:
	bool RefreshModuleInfoByClassName();

	bool GenerateCode(UClass* TargetActor, FReplicatorCodeGroup& ReplicatorCodeGroup, FString& ResultMessage);

	bool IsReplicatedActor(UClass* TargetActor);

protected:
	TArray<UClass*> GetAllUClass();

	FString GetProtoMessageOfGlobalStruct();

	TMap<FString, FModuleInfo> ModuleInfoByClassName;

	inline void ProcessHeaderFiles(const TArray<FString>& Files, const FManifestModule& ManifestModule);
};
