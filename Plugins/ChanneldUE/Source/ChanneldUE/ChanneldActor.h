// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChanneldTypes.h"
#include "ChannelDataProvider.h"
#include "ChanneldActor.generated.h"

UCLASS()
class CHANNELDUE_API AChanneldActor : public AActor, public IChannelDataProvider
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AChanneldActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EChanneldChannelType ChannelType;

	ChannelId OwningChannelId;
	bool bRemoved = false;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	//~ Begin IChannelDataProvider Interface.
	channeldpb::ChannelType GetChannelType() override;
	google::protobuf::Message* GetChannelDataTemplate() const override;
	ChannelId GetChannelId() override;
	void SetChannelId(ChannelId ChId) override;
	bool IsRemoved() override;
	void SetRemoved() override;
	bool UpdateChannelData(google::protobuf::Message* ChannelData) override;
	void OnChannelDataUpdated(const google::protobuf::Message* ChannelData) override;
	//~ End IChannelDataProvider Interface.
};
