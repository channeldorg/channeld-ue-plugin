#pragma once
#include "PropertyDecorator.h"
#include "RPCDecorator.h"
#include "Manifest.h"

static const TCHAR* ReplicatedActorDeco_ProtoStateMessageTemplate =
	LR"EOF(
message {Declare_StateMessageType} {
  {Declare_ProtoFields}
}
)EOF";

static const TCHAR* ReplicatedActorDeco_GetCode_AssignPropertyPointerTemplate =
	LR"EOF(
  {
    auto Property = CastFieldChecked<const {Declare_PropertyType}>({Ref_TargetInstanceRef}->GetClass()->FindPropertyByName(FName("{Declare_PropertyName}")));
    {Declare_PropertyPointer} = Property->ContainerPtrToValuePtr<{Declare_PropertyCPPType}>({Ref_TargetInstanceRef}.Get());
    check({Declare_PropertyPointer});
  }
)EOF";

class FReplicatedActorDecorator
{
public:
	FReplicatedActorDecorator(const UClass*);

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
	FString GetCode_AssignPropertyPointers(const FString& TargetInstance);

	/**
	 * Get protobuf package name
	 */
	FString GetProtoPackageName();

	/**
	 * Get protobuf c++ code namespace
	 */
	FString GetProtoNamespace();

	/**
	 * Get message type for replicated actor properties mapping
	 */
	FString GetProtoStateMessageType();

	/**
	 * Get code that sets whole delta state
	 */
	FString GetCode_AllPropertiesSetDeltaState(const FString& TargetInstance, const FString& FullStateName, const FString& DeltaStateName);

	/**
	 * Get code that handles state changed
	 */
	FString GetCode_AllPropertiesOnStateChange(const FString& TargetInstance, const FString& NewStateName);

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

	bool IsBlueprintGenerated();

protected:
	const UClass* Target;
	FModuleInfo ModuleBelongTo;
	TArray<TSharedPtr<FPropertyDecorator>> Properties;
	TArray<TSharedPtr<FRPCDecorator>> RPCs;

	bool bIsBlueprintGenerated;
};
