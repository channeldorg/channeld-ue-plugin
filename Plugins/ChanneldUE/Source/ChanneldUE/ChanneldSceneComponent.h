// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "ChannelDataProvider.h"
#include "unreal_common.pb.h"
#include "ChanneldSceneComponent.generated.h"


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CHANNELDUE_API UChanneldSceneComponent : public USceneComponent//, public IChannelDataProvider
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UChanneldSceneComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

	/*
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EChanneldChannelType ChannelType;
	ChannelId OwningChannelId;
	bool bRemoved = false;
	*/

	ISceneComponentStateProvider* StateProvider = nullptr;

	bool bStateChanged = false;
	channeldpb::SceneComponentState* State = nullptr;
	channeldpb::FVector* RelativeLocationState = nullptr;
	channeldpb::FVector* RelativeRotationState = nullptr;
	channeldpb::FVector* RelativeScaleState = nullptr;

	// Local -> channeld
	void OnTransformUpdated(USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport);

	// channeld -> local
	void OnStateChanged(channeldpb::SceneComponentState* NewState);

public:	

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/*
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
	*/
};
