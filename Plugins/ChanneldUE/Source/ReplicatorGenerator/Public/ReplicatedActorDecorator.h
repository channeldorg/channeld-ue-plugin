#pragma once
#include "PropertyDecorator.h"
#include "RPCDecorator.h"

static const TCHAR* ReplicatedActorDeco_ProtoStateMessageTemplate =
	LR"EOF(
message {Declare_StateMessageType} {
  {Declare_ProtoFields}
}
	)EOF";

class FReplicatedActorDecorator
{
public:
	FReplicatedActorDecorator(const UClass*);

	TArray<TSharedPtr<FPropertyDecorator>>& GetPropertyDecorators() { return Properties; }

	/**
	 * Get target actor cpp class name
	 */
	FString GetActorCppClassName();
	
	/**
	 * Get class name of generated replicator
	 * FChanneld[TargetActorName]Replicator
	 */
	FString GetReplicatorClassName(bool WithPrefix = true);

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
	FString GetCode_AllPropertiesSetDeltaState(const FString& TargetInstanceRef, const FString& FullStateRef, const FString& DeltaStateRef);

	/**
	 * Get code that handles state changed
	 */
	FString GetCode_AllPropertiesOnStateChange(const FString& TargetInstanceRef, const FString& NewStateRef);
	
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
	
protected:
	const UClass* Target;
	TArray<TSharedPtr<FPropertyDecorator>> Properties;
	TArray<TSharedPtr<FRPCDecorator>> RPCs;
};
