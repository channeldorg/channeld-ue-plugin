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
	UFUNCTION(BlueprintImplementableEvent)
	UProtoMessageObject* ProvideChannelDataTemplate() const;

	virtual const google::protobuf::Message* GetStateFromChannelData(google::protobuf::Message* ChannelData, UClass* TargetClass, uint32 NetGUID, bool& bIsRemoved);
	virtual void SetStateToChannelData(const google::protobuf::Message* State, google::protobuf::Message* ChannelData, UClass* TargetClass, uint32 NetGUID);

	bool bInitialized = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EChanneldChannelType ChannelType;
	ChannelId OwningChannelId;
	bool bRemoved = false;

	TArray< FChanneldReplicatorBase* > Replicators;

public:

	virtual void PostInitProperties() override;
	virtual void InitOnce();
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	//~ Begin IChannelDataProvider Interface.
	channeldpb::ChannelType GetChannelType() override;
	virtual google::protobuf::Message* GetChannelDataTemplate() const override;
	ChannelId GetChannelId() override;
	void SetChannelId(ChannelId ChId) override;
	bool IsRemoved() override;
	void SetRemoved() override;
	bool UpdateChannelData(google::protobuf::Message* ChannelData) override;
	void OnChannelDataUpdated(google::protobuf::Message* ChannelData) override;
	//~ End IChannelDataProvider Interface.

	TSharedPtr<google::protobuf::Message> SerializeFunctionParams(AActor* Actor, UFunction* Func, void* Params, bool& bSuccess);
	void* DeserializeFunctionParams(AActor* Actor, UFunction* Func, const std::string& ParamsPayload, bool& bSuccess, bool& bDelayRPC);
};