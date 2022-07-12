// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChanneldUE/ChanneldNetDriver.h"
#include "ChanneldUE/Test.pb.h"
#include "UObject/Object.h"
#include "TestProtoMerge.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class CHANNELDUE_API UTestProtoMerge : public UObject, public IChannelDataProvider
{
	GENERATED_BODY()

public:

	UTestProtoMerge();

	virtual void GetLifetimeReplicatedProps( TArray< class FLifetimeProperty > & OutLifetimeProps ) const override;

	UFUNCTION(BlueprintCallable)
	void SetTestText(const FString& NewValue);

	UFUNCTION(BlueprintCallable)
	void SetTestNum(const int& NewValue);

	virtual void PostLoad() override;
	// Called when the UE Editor starts up
	//virtual void PostInitProperties() override;

	//virtual void Serialize(FArchive& Ar) override;

	//~ Begin IChannelDataProvider Interface.
	virtual channeldpb::ChannelType GetChannelType() override { return channeldpb::GLOBAL; }
	virtual google::protobuf::Message* GetChannelDataTemplate() const override { return new testpb::TestChannelDataMessage; }
	virtual uint32 GetChannelId() override { return ChannelId; }
	virtual void SetChannelId(uint32 ChId) override { ChannelId = ChId; }
	virtual bool IsRemoved() override { return bRemoved; }
	virtual void SetRemoved() override { bRemoved = true; }
	virtual bool UpdateChannelData(google::protobuf::Message* ChannelData) override;
	virtual void OnChannelDataUpdated(const google::protobuf::Message* ChannelData) override;
	//~ End IChannelDataProvider Interface


private:

	uint32 ChannelId;
	bool bRemoved;

	UPROPERTY(Replicated, BlueprintSetter="SetTestText")
	FString TestText;

	UPROPERTY(Replicated, BlueprintSetter="SetTestNum")
	int TestNum;

	testpb::TestChannelDataMessage TestChannelData;

	bool TestChannelDataChanged;

};
