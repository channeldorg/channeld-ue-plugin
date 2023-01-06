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

class FReplicatedActorDecorator : public IPropertyDecoratorOwner
{
public:
	FReplicatedActorDecorator(const UClass*, const TFunction<FString()>& SetBPTargetRepName);

	virtual ~FReplicatedActorDecorator() = default;

	TArray<TSharedPtr<FPropertyDecorator>>& GetPropertyDecorators() { return Properties; }

	void Init();
	void Init(const FModuleInfo& InModuleBelongTo);

	/**
      * Get target actor name
      */
	FString GetActorName();

	/**
	 * Get target actor cpp class name
	 */
	FString GetActorCPPClassName();

	/**
	 * Get code of include target actor header
	 */
	FString GetActorHeaderIncludePath();

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
	 * Get protobuf c++ code namespace
	 */
	virtual FString GetProtoNamespace() override;

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

	FString GetDefinition_RPCParamsMessage();

	virtual bool IsBlueprintType() override;

	int32 GetRPCNum();

	FString GetCode_SerializeFunctionParams();
	FString GetCode_DeserializeFunctionParams();
	
	FString GetDeclaration_RPCParamStructNamespace();

	FString GetDeclaration_RPCParamStructs();

	FString GetInstanceRefName() const;
	void SetInstanceRefName(const FString& InstanceRefName);

	virtual FString GetCode_GetWorldRef() override;

	virtual FString GetCode_OverrideGetNetGUID();

protected:
	const UClass* Target;
	FString TargetActorName;
	FString InstanceRefName;
	FModuleInfo ModuleBelongTo;
	TArray<TSharedPtr<FPropertyDecorator>> Properties;
	TArray<TSharedPtr<FRPCDecorator>> RPCs;

	bool bIsBlueprintGenerated;
	FString ReplicatorClassName;
};
