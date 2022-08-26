// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChanneldTypes.h"
#include "ChannelDataProvider.h"
#include "ProtoMessageObject.h"
#include "Replication/ChanneldSceneComponentReplicator.h"
#include "Components/ActorComponent.h"
#include "ChanneldActor.generated.h"

UCLASS(Blueprintable, Abstract)
class CHANNELDUE_API AChanneldActor : public AActor, public IChannelDataProvider//, public ISceneComponentStateProvider
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AChanneldActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

	UFUNCTION(BlueprintImplementableEvent)
	UProtoMessageObject* ProvideChannelDataTemplate() const;

	virtual uint32 GetNetGUID();
	virtual const unrealpb::SceneComponentState* GetSceneComponentStateFromChannelData(google::protobuf::Message* ChannelData, uint32 NetGUID) PURE_VIRTUAL(GetSceneComponentStateFromChannelData, return nullptr;);
	virtual void SetSceneComponentStateToChannelData(unrealpb::SceneComponentState* State, google::protobuf::Message* ChannelData, uint32 NetGUID) PURE_VIRTUAL(SetSceneComponentStateToChannelData, );

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EChanneldChannelType ChannelType;
	ChannelId OwningChannelId;
	bool bRemoved = false;

	TArray< FChanneldSceneComponentReplicator* > SceneComponentReplicators;

public:	

	// Called every frame
	virtual void Tick(float DeltaTime) override;

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
