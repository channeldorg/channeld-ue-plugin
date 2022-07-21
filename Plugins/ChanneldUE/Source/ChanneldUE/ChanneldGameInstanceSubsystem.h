// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChanneldTypes.h"
#include "Tickable.h"
#include "google/protobuf/message.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ChanneldGameInstanceSubsystem.generated.h"

class UMessageWrapper;
class UCustomProtoType;
class UChanneldConnection;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAuth, int32, AuthResult, int32, ConnId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnCreateChannel, int32, ChId, int32, ChannelType, FString, Metadata, int32, OwnerConnId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnSubToChannel, int32, Chld, int32, ConnType, int32, ChannelType, int32, ConnId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDataUpdate, int32, Chld, UMessageWrapper*, MsgWrapper, int32, contextConnId);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnceOnAuth, int32, AuthResult, int32, ConnId);
DECLARE_DYNAMIC_DELEGATE_FourParams(FOnceOnCreateChannel, int32, ChId, int32, ChannelType, FString, Metadata, int32, OwnerConnId);
DECLARE_DYNAMIC_DELEGATE_FourParams(FOnceOnSubToChannel, int32, Chld, int32, ConnType, int32, ChannelType, int32, ConnId);

UCLASS()
class CHANNELDUE_API UChanneldGameInstanceSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
		FOnAuth OnAuth;

	UPROPERTY(BlueprintAssignable)
		FOnCreateChannel OnCreateChannel;

	UPROPERTY(BlueprintAssignable)
		FOnSubToChannel OnSubToChannel;

	UPROPERTY(BlueprintAssignable)
		FOnDataUpdate OnDataUpdate;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return !IsTemplate(); };
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMyScoreSubsystem, STATGROUP_Tickables); }

	/*
	 * Is connected to server
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Channeld|Net")
		void IsConnected(bool& bConnected);

	/*
	 * Get own ConnId from ConnectionInstance
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Channeld|Net")
		void GetConnId(int32& ConnId);

	UFUNCTION(BlueprintCallable, Meta = (AutoCreateRefTerm = "AuthCallback"), Category = "Channeld|Net")
		void ConnectServer(bool& Success, FString& Error, FString Host, int32 Port, const FOnceOnAuth& AuthCallback);

	UFUNCTION(BlueprintCallable, Meta = (AutoCreateRefTerm = "Callback"), Category = "Channeld|Net")
		void CreateChannel(int32 ChannelType, FString Metadata, UMessageWrapper* InitData, const FOnceOnCreateChannel& Callback);

	UFUNCTION(BlueprintCallable, Meta = (AutoCreateRefTerm = "Callback"), Category = "Channeld|Net")
		void SubToChannel(int32 ChId, const FOnceOnSubToChannel& Callback);

	UFUNCTION(BlueprintCallable, Meta = (AutoCreateRefTerm = "Callback"), Category = "Channeld|Net")
		void SubConnectionToChannel(int32 TargetConnId, int32 ChId, const FOnceOnSubToChannel& Callback);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Net")
		void SendDataUpdate(int32 ChId, UMessageWrapper* MsgWrapper);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf")
		void SpawnMessageByProtoType(UMessageWrapper*& MsgWrapper, bool& bSuccess, FString ProtoName);

	/*
	 * Time utils
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Channeld|Time")
		void GetNowTimestamp(int64& Now);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Channeld|Time")
		void TimestampToDateTime(FDateTime& DateTime, int64 Timestamp);

protected:
	UPROPERTY()
		UChanneldConnection* ConnectionInstance = nullptr;

	void HandleAuthResult(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleCreateChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleSubToChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleChannelDataUpdate(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
};
