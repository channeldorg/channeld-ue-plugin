// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Channeld.pb.h"
#include "ChanneldTypes.h"
#include "Tickable.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ChanneldGameInstanceSubsystem.generated.h"

class UProtoMessageObject;
class UChanneldConnection;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAuth, int32, AuthResult, int32, ConnId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnCreateChannel, int32, ChId, EChanneldChannelType, ChannelType, FString, Metadata, int32, OwnerConnId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRemoveChannel, int32, ChId, int32, RemovedChannelId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnListChannel, const TArray<FListedChannelInfo>&, Channels);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnSubToChannel, int32, ChId, EChanneldChannelType, ChannelType, int32, ConnId, EChanneldConnectionType, ConnType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnUnsubFromChannel, int32, ChId, EChanneldChannelType, ChannelType, int32, ConnId, EChanneldConnectionType, ConnType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnDataUpdate, int32, ChId, EChanneldChannelType, ChannelType, UProtoMessageObject*, MessageObject, int32, ContextConnId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnUserSpaceMessage, int32, ChId, int32, ConnId, UProtoMessageObject*, MessageObject);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnceOnAuth, int32, AuthResult, int32, ConnId);
DECLARE_DYNAMIC_DELEGATE_FourParams(FOnceOnCreateChannel, int32, ChId, EChanneldChannelType, ChannelType, FString, Metadata, int32, OwnerConnId);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnceOnRemoveChannel, int32, ChId, int32, RemovedChannelId);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnceOnListChannel, const TArray<FListedChannelInfo>&, Channels);
DECLARE_DYNAMIC_DELEGATE_FourParams(FOnceOnSubToChannel, int32, ChId, EChanneldChannelType, ChannelType, int32, ConnId, EChanneldConnectionType, ConnType);
DECLARE_DYNAMIC_DELEGATE_FourParams(FOnceOnUnsubFromChannel, int32, ChId, EChanneldChannelType, ChannelType, int32, ConnId, EChanneldConnectionType, ConnType);

UCLASS(Meta = (DisplayName = "Channeld"))
class CHANNELDUE_API UChanneldGameInstanceSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:

	TSharedRef<ChannelId> LowLevelSendToChannelId = MakeShared<ChannelId>(GlobalChannelId);

	UPROPERTY(BlueprintAssignable)
		FOnAuth OnAuth;

	UPROPERTY(BlueprintAssignable)
		FOnCreateChannel OnCreateChannel;

	UPROPERTY(BlueprintAssignable)
		FOnSubToChannel OnSubToChannel;

	UPROPERTY(BlueprintAssignable)
		FOnUnsubFromChannel OnUnsubFromChannel;

	UPROPERTY(BlueprintAssignable)
		FOnDataUpdate OnDataUpdate;

	UPROPERTY(BlueprintAssignable)
		FOnUserSpaceMessage OnUserSpaceMessage;

	UPROPERTY(BlueprintAssignable)
		FOnRemoveChannel OnRemoveChannel;

	UPROPERTY(BlueprintAssignable)
		FOnListChannel OnListChannel;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return !IsTemplate(); };
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UChanneldGameInstanceSubsystem, STATGROUP_Tickables); }

	void InitConnection();

	UChanneldConnection* GetConnection() { return ConnectionInstance; }

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
		bool IsServerConnection();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Channeld|Net")
		bool IsClientConnection();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Channeld|Net")
		EChanneldChannelType GetChannelTypeByChId(int32 ChId);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Channeld|Net")
		TArray<FSubscribedChannelInfo> GetSubscribedChannels();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Channeld|Net")
		TArray<FListedChannelInfo> GetListedChannels();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Channeld|Net")
		const TMap<int32, FSubscribedChannelInfo> GetSubscribedsOnOwnedChannel(bool& bSuccess, int32 ChId);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Channeld|Net")
		FSubscribedChannelInfo GetSubscribedOnOwnedChannelByConnId(bool& bSuccess, int32 ChId, int32 ConnId);

	UFUNCTION(BlueprintCallable, Meta = (AutoCreateRefTerm = "AuthCallback"), Category = "Channeld|Net")
		void ConnectToChanneld(bool& Success, FString& Error, FString Host, int32 Port, const FOnceOnAuth& AuthCallback, bool bInitAsClient = true);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Net")
		void DisconnectFromChanneld(bool bFlushAll = true);

	UFUNCTION(BlueprintCallable, Meta = (AutoCreateRefTerm = "Callback"), Category = "Channeld|Net")
		void CreateChannel(EChanneldChannelType ChannelType, FString Metadata, UProtoMessageObject* InitData, const FOnceOnCreateChannel& Callback);

	UFUNCTION(BlueprintCallable, Meta = (AutoCreateRefTerm = "Callback"), Category = "Channeld|Net")
		void RemoveChannel(int32 ChannelToRemove, const FOnceOnRemoveChannel& Callback);

	UFUNCTION(BlueprintCallable, Meta = (AutoCreateRefTerm = "MetadataFilters, Callback"), Category = "Channeld|Net")
		void ListChannel(EChanneldChannelType ChannelTypeFilter, const TArray<FString>& MetadataFilters, const FOnceOnListChannel& Callback);

	UFUNCTION(BlueprintCallable, Meta = (AutoCreateRefTerm = "Callback"), Category = "Channeld|Net")
		void SubToChannel(int32 ChId, const FOnceOnSubToChannel& Callback);

	UFUNCTION(BlueprintCallable, Meta = (AutoCreateRefTerm = "Callback"), Category = "Channeld|Net")
		void SubConnectionToChannel(int32 TargetConnId, int32 ChId, const FOnceOnSubToChannel& Callback);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Net")
		void SendDataUpdate(int32 ChId, UProtoMessageObject* MessageObject);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Net", Meta = (ToolTip = "Only run on server"))
		void ServerBroadcast(int32 ChId, int32 ClientConnId, UProtoMessageObject* MessageObject, EChanneldBroadcastType BroadcastType);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf")
		void RegisterChannelTypeByFullName(EChanneldChannelType ChannelType, FString ProtobufFullName);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf")
		void CreateMessageObjectByChannelType(UProtoMessageObject*& MessageObject, bool& bSuccess, EChanneldChannelType ChannelType);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf")
		void CreateMessageObjectByFullName(UProtoMessageObject*& MessageObject, bool& bSuccess, FString ProtobufFullName);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Channeld|View")
		UChannelDataView* GetChannelDataView();

	UFUNCTION(BlueprintCallable, Category = "Channeld|View")
	void SetLowLevelSendToChannelId(int32 ChId);

protected:
	UPROPERTY()
		UChanneldConnection* ConnectionInstance = nullptr;

	TMap<EChanneldChannelType, FString> ChannelTypeToProtoFullNameMapping;

	google::protobuf::Message* CreateProtoMessageByFullName(const std::string ProtobufFullName);

	void HandleAuthResult(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleCreateChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleRemoveChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleListChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleSubToChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleUnsubFromChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleChannelDataUpdate(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);

	class UChanneldNetDriver* GetNetDriver();
};


