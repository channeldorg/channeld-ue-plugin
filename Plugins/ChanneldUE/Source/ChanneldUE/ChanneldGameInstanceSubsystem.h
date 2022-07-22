// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Channeld.pb.h"
#include "ChanneldTypes.h"
#include "Tickable.h"
#include "google/protobuf/message.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ChanneldGameInstanceSubsystem.generated.h"

class UProtoMessageObject;
class UCustomProtoType;
class UChanneldConnection;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAuth, int32, AuthResult, int32, ConnId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnCreateChannel, int32, ChId, EChanneldChannelType, ChannelType, FString, Metadata, int32, OwnerConnId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnSubToChannel, int32, Chld, EChanneldChannelType, ChannelType, int32, ConnId, int32, ConnType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnDataUpdate, int32, Chld, EChanneldChannelType, ChannelType, UProtoMessageObject*, MessageObject, int32, contextConnId);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnceOnAuth, int32, AuthResult, int32, ConnId);
DECLARE_DYNAMIC_DELEGATE_FourParams(FOnceOnCreateChannel, int32, ChId, EChanneldChannelType, ChannelType, FString, Metadata, int32, OwnerConnId);
DECLARE_DYNAMIC_DELEGATE_FourParams(FOnceOnSubToChannel, int32, Chld, EChanneldChannelType, ChannelType, int32, ConnId, int32, ConnType);

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
		bool IsConnected();

	/*
	 * Get own ConnId from ConnectionInstance
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Channeld|Net")
		int32 GetConnId();



	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Channeld|Net")
		EChanneldChannelType GetChannelTypeByChId(int32 ChId);

	FORCEINLINE channeldpb::ChannelType  GetProtoChannelTypeByChId(int32 ChId);

	UFUNCTION(BlueprintCallable, Meta = (AutoCreateRefTerm = "AuthCallback"), Category = "Channeld|Net")
		void ConnectToChanneld(bool& Success, FString& Error, FString Host, int32 Port, const FOnceOnAuth& AuthCallback);

	UFUNCTION(BlueprintCallable, Meta = (AutoCreateRefTerm = "Callback"), Category = "Channeld|Net")
		void CreateChannel(EChanneldChannelType ChannelType, FString Metadata, UProtoMessageObject* InitData, const FOnceOnCreateChannel& Callback);

	UFUNCTION(BlueprintCallable, Meta = (AutoCreateRefTerm = "Callback"), Category = "Channeld|Net")
		void SubToChannel(int32 ChId, const FOnceOnSubToChannel& Callback);

	UFUNCTION(BlueprintCallable, Meta = (AutoCreateRefTerm = "Callback"), Category = "Channeld|Net")
		void SubConnectionToChannel(int32 TargetConnId, int32 ChId, const FOnceOnSubToChannel& Callback);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Net")
		void SendDataUpdate(int32 ChId, UProtoMessageObject* MessageObject);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf")
		bool RegisterChannelTypeByFullName(EChanneldChannelType ChannelType, FString ProtobufFullName);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf")
		void CreateMessageObjectByChannelType(UProtoMessageObject*& MessageObject, bool& bSuccess, EChanneldChannelType ChannelType);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf")
		void CreateMessageObjectByFullName(UProtoMessageObject*& MessageObject, bool& bSuccess, FString ProtobufFullName);

protected:
	UPROPERTY()
		UChanneldConnection* ConnectionInstance = nullptr;

	TMap<channeldpb::ChannelType, const google::protobuf::Message*> ChannelTypeToMsgPrototypeMapping;

	void HandleAuthResult(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleCreateChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleSubToChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleChannelDataUpdate(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
};
