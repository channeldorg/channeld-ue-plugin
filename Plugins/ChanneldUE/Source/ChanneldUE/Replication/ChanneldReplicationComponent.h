#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChanneldTypes.h"
#include "ChannelDataProvider.h"
#include "ProtoMessageObject.h"
#include "Replication/ChanneldSceneComponentReplicator.h"
#include "Components/ActorComponent.h"
#include "ChanneldReplicationComponent.generated.h"

// Responsible for replicating the owning Actor and its replicated component via ChannelDataUpdate
UCLASS(Abstract, ClassGroup = "Channeld")
class CHANNELDUE_API UChanneldReplicationComponent : public UActorComponent, public IChannelDataProvider
{
	GENERATED_BODY()

public:	
	UChanneldReplicationComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:

	UFUNCTION(BlueprintImplementableEvent)
	UProtoMessageObject* ProvideChannelDataTemplate() const;

	virtual const unrealpb::SceneComponentState* GetSceneComponentStateFromChannelData(google::protobuf::Message* ChannelData, uint32 NetGUID) PURE_VIRTUAL(GetSceneComponentStateFromChannelData, return nullptr;);
	virtual void SetSceneComponentStateToChannelData(unrealpb::SceneComponentState * State, google::protobuf::Message * ChannelData, uint32 NetGUID) PURE_VIRTUAL(SetSceneComponentStateToChannelData, );

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EChanneldChannelType ChannelType;
	ChannelId OwningChannelId;
	bool bRemoved = false;

	TArray< FChanneldSceneComponentReplicator* > SceneComponentReplicators;

public:

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

};