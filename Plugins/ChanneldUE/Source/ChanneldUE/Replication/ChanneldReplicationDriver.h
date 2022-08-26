#pragma once

#include "CoreMinimal.h"
//#include "ReplicationGraph.h"
#include "BasicReplicationGraph.h"
#include "ChanneldReplicationDriver.generated.h"

UCLASS(config = ChanneldUE)
class CHANNELDUE_API UChanneldReplicationDriver : public UBasicReplicationGraph//UReplicationDriver
{
	GENERATED_BODY()

public:
	UChanneldReplicationDriver();

	/*
	//~ Begin UReplicationDriver Interface
	virtual void SetRepDriverWorld(UWorld* InWorld) override;
	virtual void InitForNetDriver(UNetDriver* InNetDriver) override;
	virtual void InitializeActorsInWorld(UWorld* InWorld) override;
	virtual void TearDown() { MarkPendingKill(); }
	virtual void ResetGameWorldState() override;
	virtual void AddClientConnection(UNetConnection* NetConnection) override;
	virtual void RemoveClientConnection(UNetConnection* NetConnection) override;
	virtual void AddNetworkActor(AActor* Actor) override;
	virtual void RemoveNetworkActor(AActor* Actor) override;
	virtual void ForceNetUpdate(AActor* Actor) override;
	virtual void FlushNetDormancy(AActor* Actor, bool WasDormInitial) override;
	virtual void NotifyActorTearOff(AActor* Actor) override;
	virtual void NotifyActorFullyDormantForConnection(AActor* Actor, UNetConnection* Connection) override;
	virtual void NotifyActorDormancyChange(AActor* Actor, ENetDormancy OldDormancyState) override;
	virtual void NotifyDestructionInfoCreated(AActor* Actor, FActorDestructionInfo& DestructionInfo) override;
	virtual void SetRoleSwapOnReplicate(AActor* Actor, bool bSwapRoles) override;
	virtual bool ProcessRemoteFunction(class AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, UObject* SubObject) { return false; }
	virtual int32 ServerReplicateActors(float DeltaSeconds) override;
	virtual void PostTickDispatch() { }
	//~ End UReplicationDriver Interface
	*/
};