#pragma once
#include "IPropertyDecoratorOwner.h"
#include "PropertyDecorator.h"
#include "RPCDecorator.h"
#include "Manifest.h"

static const TCHAR* ActorComp_GetNetGUIDTemplate =
	LR"EOF(
uint32 {Declare_ReplicatorClassName}::GetNetGUID()
{
  if (!NetGUID.IsValid())
  {
    if ({Ref_TargetInstanceRef}.IsValid())
    {
      UWorld* World = {Ref_TargetInstanceRef}->GetWorld();
      if (World && World->GetNetDriver())
      {
        NetGUID = World->GetNetDriver()->GuidCache->GetNetGUID({Ref_TargetInstanceRef}->GetOwner());
      }
    }
  }
  return NetGUID.Value;
}
)EOF";

static const TCHAR* GameState_GetNetGUIDTemplate =
	LR"EOF(
uint32 {Declare_ReplicatorClassName}::GetNetGUID()
{
  return 1;
}
)EOF";

static const TCHAR* ActorDecor_GetStateFromChannelData =
	LR"EOF(
if({Code_Condition}) {
  auto States = {Declaration_ChannelDataMessage}->mutable_{Definition_ChannelDataFieldName}();
  if (States->contains(NetGUID))
  {
    bIsRemoved = false;
    return &States->at(NetGUID);
  }
}
)EOF";

static const TCHAR* ActorDecor_ChannelDataProcessorMergeLoop =
	LR"EOF(
for (auto& Pair : Src->{Definition_ChannelDataFieldName}())
{
{Code_MergeLoopInner}
}
)EOF";

static const TCHAR* ActorDecor_ChannelDataProcessorMerge_Erase =
	LR"EOF(
if (Pair.second.removed())
{
{Code_MergeEraseInner}
}
else
{
{Code_DoMerge}
}
)EOF";

static const TCHAR* ActorDecor_ChannelDataProcessorMerge_DoMarge =
	LR"EOF(
if (Dst->{Definition_ChannelDataFieldName}().contains(Pair.first))
{
  Dst->mutable_{Definition_ChannelDataFieldName}()->at(Pair.first).MergeFrom(Pair.second);
}
else
{
  Dst->mutable_{Definition_ChannelDataFieldName}()->emplace(Pair.first, Pair.second);
}
)EOF";

static const TCHAR* ActorDecor_ChannelDataProcessorMerge_Singleton =
	LR"EOF(
if (Src->has_{Definition_ChannelDataFieldName}())
{
  Dst->mutable_{Definition_ChannelDataFieldName}()->MergeFrom(Src->{Definition_ChannelDataFieldName}());
}
)EOF";

static const TCHAR* ActorDecor_GetStateFromChannelData_Singleton =
	LR"EOF(
if({Code_Condition}) {
  bIsRemoved = false;
  return {Declaration_ChannelDataMessage}->mutable_{Definition_ChannelDataFieldName}();
}
)EOF";

static const TCHAR* ActorDecor_SetStateToChannelData =
	LR"EOF(
if({Code_Condition}) {
    auto States = {Declaration_ChannelDataMessage}->mutable_{Definition_ChannelDataFieldName}();
    (*States)[NetGUID] = *static_cast<const {Definition_ProtoNamespace}::{Definition_ProtoStateMsgName}*>(State);
}
)EOF";

static const TCHAR* ActorDecor_SetStateToChannelData_Singleton =
	LR"EOF(
if({Code_Condition}) {
  {Declaration_ChannelDataMessage}->mutable_{Definition_ChannelDataFieldName}()->MergeFrom(*static_cast<const {Definition_ProtoNamespace}::{Definition_ProtoStateMsgName}*>(State));
}
)EOF";

class FReplicatedActorDecorator : public IPropertyDecoratorOwner
{
public:
	FReplicatedActorDecorator(
		const UClass* TargetActorClass,
		const TFunction<void(FString& TargetActorName, bool IsActorNameCompilable)>& SetCompilableName,
		FString ProtoPackageName,
		FString GoPackageName,
		bool IsSingleton,
		bool IsChanneldUEBuiltinType,
		bool IsSkipGenReplicator,
		bool IsSkipGenChannelDataState
	);

	virtual ~FReplicatedActorDecorator() = default;

	TArray<TSharedPtr<FPropertyDecorator>>& GetPropertyDecorators() { return Properties; }

	/**
	 * Create property decorators and RPC decorators
	 */
	void InitPropertiesAndRPCs();

	/**
	 * Is target actor class a blueprint class
	 */
	virtual bool IsBlueprintType() override;

	/**
	 * Some classes are only one instance in the whole server cluster, such as GameState.
	 * Normally, the singleton instance is owned by the master server.
	 */
	virtual bool IsSingleton();

	/**
	 * Is the replicator of the target class has been implemented by ChanneldUE,
	 * and the state message is contained in the unreal_common.proto.
	 *
	 * TODO: The list of builtin types should be maintained by ChanneldUE module.
	 */
	virtual bool IsChanneldUEBuiltinType();

	virtual bool IsSkipGenReplicator();

	virtual bool IsSkipGenChannelDataState();

	/**
	 * Set module info if the target actor class is a cpp class.
	 * Please call this function before calling GetActorHeaderIncludePath().
	 */
	void SetModuleInfo(const FModuleInfo& InModuleBelongTo);

	FORCEINLINE const UClass* GetTargetClass() const { return TargetClass; }

	/**
      * Get target actor name
      */
	FString GetActorName();

	/**
	 * Get target actor asset path
	 */
	FString GetActorPathName();

	/**
      * Get target origin actor name
      */
	FString GetOriginActorName();

	/**
      * Get target outer package path name
      */
	FString GetPackagePathName();

	/**
	 * Get target actor cpp class name
	 */
	FString GetActorCPPClassName();

	/**
	 * Get code of include target actor header
	 */
	FString GetIncludeActorHeaderPath();

	/**
     * Get code of additional include files 
     */
	FString GetAdditionalIncludeFiles();

	virtual TArray<FString> GetAdditionalIncludes() override
	{
		return TArray<FString>();
	}

	/**
	 * Get class name of generated replicator
	 * FChanneld[TargetActorName]Replicator
	 */
	FString GetReplicatorClassName(bool WithPrefix = true);

	/**
	 * Get all declaration of externally inaccessible property pointer
	 */
	FString GetCode_IndirectlyAccessiblePropertyPtrDeclarations();

	/**
	 * Get all code of assign property pointer
	 */
	FString GetCode_AssignPropertyPointers();

	/**
	 * Get protobuf package name
	 */
	virtual FString GetProtoPackageName() override;

	/**
	 * Get protobuf package path for generated Go code.
	 * If the state's package name equals the channel data's package name, return an empty string.
	 */
	FString GetProtoPackagePathGo(const FString& ChannelDataPackageName);

	/**
	 * Get protobuf c++ code namespace
	 */
	virtual FString GetProtoNamespace() override;

	/**
	 * Get protobuf definitions file name
	 */
	virtual FString GetProtoDefinitionsFileName();

	/**
	 * Get go package name. Used for go proto code generation
	 */
	virtual FString GetGoPackageImportPath();

	/**
	 * Get message type for replicated actor properties mapping
	 */
	virtual FString GetProtoStateMessageType() override;

	/**
	 * Get code that sets whole delta state
	 */
	FString GetCode_AllPropertiesSetDeltaState(const FString& FullStateName, const FString& DeltaStateName);

	/**
	 * Get code that handles state changed
	 */
	FString GetCode_AllPropertiesOnStateChange(const FString& NewStateName);

	/**
	 * Get protobuf message definition
	 *
	 * For example:
	 *   message CharacterState {
	 *     optional bool bIsCrouched = 1;
     *   }
     *   
	 */
	FString GetDefinition_ProtoStateMessage();

	int32 GetRPCNum();

	FString GetCode_SerializeFunctionParams();
	FString GetCode_DeserializeFunctionParams();

	FString GetDeclaration_RPCParamStructs();
	FString GetDefinition_RPCParamProtoDefinitions();

	FString GetInstanceRefName() const;
	void SetInstanceRefName(const FString& InstanceRefName);

	virtual FString GetCode_GetWorldRef() override;

	virtual FString GetCode_OverrideGetNetGUID();

	virtual void SetConstClassPathFNameVarName(const FString& VarName);

	virtual FString GetDefinition_ChannelDataFieldNameProto();
	virtual FString GetDefinition_ChannelDataFieldNameCpp();
	virtual FString GetDefinition_ChannelDataFieldNameGo();

	virtual FString GetCode_ConstPathFNameVarDecl();

	virtual FString GetCode_ChannelDataProcessor_IsTargetClass();

	virtual FString GetCode_ChannelDataProcessor_Merge(const TArray<TSharedPtr<FReplicatedActorDecorator>>& ActorChildren);

	virtual FString GetCode_ChannelDataProcessor_GetStateFromChannelData(const FString& ChannelDataMessageName);

	virtual FString GetCode_ChannelDataProcessor_SetStateToChannelData(const FString& ChannelDataMessageName);

	virtual FString GetCode_ChannelDataProtoFieldDefinition(const int32& Index);

protected:
	const UClass* TargetClass;
	FString TargetActorCompilableName;
	FString InstanceRefName;
	FString ProtoPackageName;
	FString GoPackageImportPath;
	FModuleInfo ModuleBelongTo;
	TArray<TSharedPtr<FPropertyDecorator>> Properties;
	TArray<TSharedPtr<FRPCDecorator>> RPCs;

	bool bSingleton;
	bool bChanneldUEBuiltinType;
	bool bSkipGenReplicator;
	bool bSkipGenChannelDataState;
	
	bool bIsBlueprintGenerated;
	FString ReplicatorClassName;

	FString VariableName_ConstClassPathFName;
};
