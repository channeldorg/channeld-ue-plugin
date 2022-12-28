#pragma once
#include "IPropertyDecoratorOwner.h"
#include "PropertyDecorator.h"
#include "RPCDecorator.h"
#include "Manifest.h"

class FReplicatedActorDecorator : public IPropertyDecoratorOwner
{
public:

	FReplicatedActorDecorator(const UClass*);
	
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
	FString GetCode_AssignPropertyPointers(const FString& TargetInstance);

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
	 
	FString GetDefinition_RPCParamsMessage();

	virtual bool IsBlueprintType() override;

	int32 GetRPCNum();

	FString GetCode_SerializeFunctionParams();
	FString GetCode_DeserializeFunctionParams();
	
	FString GetDeclaration_RPCParamStructs();

protected:
	const UClass* Target;
	FModuleInfo ModuleBelongTo;
	TArray<TSharedPtr<FPropertyDecorator>> Properties;
	TArray<TSharedPtr<FRPCDecorator>> RPCs;

	bool bIsBlueprintGenerated;
};
