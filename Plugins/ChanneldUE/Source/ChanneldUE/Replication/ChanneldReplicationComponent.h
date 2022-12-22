#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChanneldTypes.h"
#include "ChannelDataProvider.h"
#include "ProtoMessageObject.h"
#include "Replication/ChanneldReplicatorBase.h"
#include "Replication/ChanneldSceneComponentReplicator.h"
#include "Components/ActorComponent.h"
#include "google/protobuf/message.h"
#include "ChanneldReplicationComponent.generated.h"

// Responsible for replicating the owning Actor and its replicated components via ChannelDataUpdate
UCLASS(Abstract, ClassGroup = "Channeld")
class CHANNELDUE_API UChanneldReplicationComponent : public UActorComponent, public IChannelDataProvider
{
	GENERATED_BODY()

public:	
	UChanneldReplicationComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual const google::protobuf::Message* GetStateFromChannelData(google::protobuf::Message* ChannelData, UClass* TargetClass, uint32 NetGUID, bool& bIsRemoved);
	virtual void SetStateToChannelData(const google::protobuf::Message* State, google::protobuf::Message* ChannelData, UClass* TargetClass, uint32 NetGUID);

	bool bInitialized = false;
	bool bUninitialized = false;
	// UPROPERTY(EditAnywhere, BlueprintReadWrite)
	// EChanneldChannelType ChannelType;
	// ChannelId OwningChannelId;
	bool bRemoved = false;

	TArray< TUniquePtr<FChanneldReplicatorBase> > Replicators;

public:

	virtual void PostInitProperties() override;
	virtual void BeginPlay() override;
	virtual void InitOnce();
	virtual void UninitOnce();
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	/* Only EndPlay() will be triggered on the server when the Actor is being destroy!
	virtual void DestroyComponent(bool bPromoteChildren = false) override;
	virtual void BeginDestroy() override;
	*/
	
	//~ Begin IChannelDataProvider Interface.
	virtual UObject* GetTargetObject() override {return GetOwner();}
	// channeldpb::ChannelType GetChannelType() override;
	// virtual google::protobuf::Message* GetChannelDataTemplate() const override;
	// ChannelId GetChannelId() override;
	// void SetChannelId(ChannelId ChId) override;
	virtual bool IsRemoved() override;
	virtual void SetRemoved(bool bInRemoved) override;
	virtual bool UpdateChannelData(google::protobuf::Message* ChannelData) override;
	virtual void OnChannelDataUpdated(google::protobuf::Message* ChannelData) override;
	//~ End IChannelDataProvider Interface.

	TSharedPtr<google::protobuf::Message> SerializeFunctionParams(AActor* Actor, UFunction* Func, void* Params, bool& bSuccess);
	TSharedPtr<void> DeserializeFunctionParams(AActor* Actor, UFunction* Func, const std::string& ParamsPayload, bool& bSuccess, bool& bDelayRPC);
};