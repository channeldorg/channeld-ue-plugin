// Fill out your copyright notice in the Description page of Project Settings.


#include "ChanneldGameInstanceSubsystem.h"
#include "ChanneldConnection.h"
#include "ProtoMessageObject.h"
#include "ChanneldNetDriver.h"
#include "Kismet/GameplayStatics.h"
#include "ChanneldUtils.h"
#include "ChanneldSettings.h"

void UChanneldGameInstanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	InitConnection();
}

void UChanneldGameInstanceSubsystem::Deinitialize()
{
	if (!ConnectionInstance)
	{
		return;
	}

	if (ChannelDataView)
	{
		ChannelDataView->Unintialize();
	}

	ConnectionInstance->RemoveMessageHandler((uint32)channeldpb::AUTH, this);
	ConnectionInstance->RemoveMessageHandler((uint32)channeldpb::CREATE_CHANNEL, this);
	ConnectionInstance->RemoveMessageHandler((uint32)channeldpb::REMOVE_CHANNEL, this);
	ConnectionInstance->RemoveMessageHandler((uint32)channeldpb::LIST_CHANNEL, this);
	ConnectionInstance->RemoveMessageHandler((uint32)channeldpb::SUB_TO_CHANNEL, this);
	ConnectionInstance->RemoveMessageHandler((uint32)channeldpb::UNSUB_FROM_CHANNEL, this);
	ConnectionInstance->RemoveMessageHandler((uint32)channeldpb::CHANNEL_DATA_UPDATE, this);
	// ConnectionInstance->OnUserSpaceMessageReceived.RemoveAll(this);
	ConnectionInstance->Disconnect();

	Super::Deinitialize();
}

void UChanneldGameInstanceSubsystem::Tick(float DeltaTime)
{
	// Don't tick the connection if the net driver exists - the tick will be handled in the net driver.
	if (GetNetDriver() == nullptr && ConnectionInstance && ConnectionInstance->IsConnected())
	{
		ConnectionInstance->TickIncoming();
		ConnectionInstance->TickOutgoing();
	}
}

void UChanneldGameInstanceSubsystem::InitConnection()
{
	if (ConnectionInstance)
	{
		return;
	}

	/*
	auto ChanneldNetDriver = GetNetDriver();
	if (ChanneldNetDriver)
	{
		// Share the same ChanneldConnection with the net driver if it exists.
		// In the Play as Client net game, NetDriver::InitBase is called before the creation of PendingNetGame.
		ConnectionInstance = ChanneldNetDriver->GetConnToChanneld();
	}
	else
	{
		ConnectionInstance = NewObject<UChanneldConnection>(this);
	}
	*/
	ConnectionInstance = GEngine->GetEngineSubsystem<UChanneldConnection>();

	ConnectionInstance->AddMessageHandler((uint32)channeldpb::AUTH, this, &UChanneldGameInstanceSubsystem::HandleAuthResult);
	ConnectionInstance->AddMessageHandler((uint32)channeldpb::CREATE_CHANNEL, this, &UChanneldGameInstanceSubsystem::HandleCreateChannel);
	ConnectionInstance->AddMessageHandler((uint32)channeldpb::REMOVE_CHANNEL, this, &UChanneldGameInstanceSubsystem::HandleRemoveChannel);
	ConnectionInstance->AddMessageHandler((uint32)channeldpb::LIST_CHANNEL, this, &UChanneldGameInstanceSubsystem::HandleListChannel);
	ConnectionInstance->AddMessageHandler((uint32)channeldpb::SUB_TO_CHANNEL, this, &UChanneldGameInstanceSubsystem::HandleSubToChannel);
	ConnectionInstance->AddMessageHandler((uint32)channeldpb::UNSUB_FROM_CHANNEL, this, &UChanneldGameInstanceSubsystem::HandleUnsubFromChannel);
	ConnectionInstance->AddMessageHandler((uint32)channeldpb::CHANNEL_DATA_UPDATE, this, &UChanneldGameInstanceSubsystem::HandleChannelDataUpdate);

	//ConnectionInstance->OnUserSpaceMessageReceived.AddUObject(this, &UChanneldGameInstanceSubsystem::OnUserSpaceMessageReceived);
	ConnectionInstance->RegisterMessageHandler(unrealpb::ANY, new google::protobuf::Any, this, &UChanneldGameInstanceSubsystem::HandleUserSpaceAnyMessage);
}

bool UChanneldGameInstanceSubsystem::IsConnected()
{
	return ConnectionInstance && ConnectionInstance->IsConnected();
}

int32 UChanneldGameInstanceSubsystem::GetConnId()
{
	if (ConnectionInstance)
	{
		return ConnectionInstance->GetConnId();
	}
	return 0;
}

bool UChanneldGameInstanceSubsystem::IsServerConnection()
{
	return ConnectionInstance && ConnectionInstance->IsServer();
}

bool UChanneldGameInstanceSubsystem::IsClientConnection()
{
	return ConnectionInstance && ConnectionInstance->IsClient();
}

bool UChanneldGameInstanceSubsystem::IsAuthenticated()
{
	return ConnectionInstance && ConnectionInstance->IsAuthenticated();
}

EChanneldChannelType UChanneldGameInstanceSubsystem::GetChannelTypeByChId(int32 ChId)
{
	if (!ensureMsgf(ConnectionInstance, TEXT("Need to call ConnectToChanneld first!")))
	{
		return EChanneldChannelType::ECT_Unknown;
	}

	FSubscribedChannelInfo* SubscribedChannelInfo = ConnectionInstance->SubscribedChannels.Find(ChId);
	return SubscribedChannelInfo != nullptr ? SubscribedChannelInfo->ChannelType : EChanneldChannelType::ECT_Unknown;
}

FString UChanneldGameInstanceSubsystem::GetChannelTypeNameByChId(int32 ChId)
{
	EChanneldChannelType Type = GetChannelTypeByChId(ChId);
	return StaticEnum<EChanneldChannelType>()->GetNameStringByValue((int64)Type);
}

TArray<FSubscribedChannelInfo> UChanneldGameInstanceSubsystem::GetSubscribedChannels()
{
	if (ensureMsgf(ConnectionInstance, TEXT("Need to call ConnectToChanneld first!")))
	{
		TArray<FSubscribedChannelInfo> Result;
		ConnectionInstance->SubscribedChannels.GenerateValueArray(Result);
		return Result;
	}
	const auto EmptyResult = TArray<FSubscribedChannelInfo>();
	return EmptyResult;
}

bool UChanneldGameInstanceSubsystem::HasSubscribedToChannel(int32 ChId)
{
	if (ConnectionInstance)
	{
		return ConnectionInstance->SubscribedChannels.Contains(ChId);
	}
	return false;
}

bool UChanneldGameInstanceSubsystem::HasOwnedChannel(int32 ChId)
{
	if (ConnectionInstance)
	{
		return ConnectionInstance->OwnedChannels.Contains(ChId);
	}
	return false;
}

TArray<FListedChannelInfo> UChanneldGameInstanceSubsystem::GetListedChannels()
{
	if (ensureMsgf(ConnectionInstance, TEXT("Need to call ConnectToChanneld first!")))
	{
		TArray<FListedChannelInfo> Result;
		ConnectionInstance->ListedChannels.GenerateValueArray(Result);
		return Result;
	}
	const auto EmptyResult = TArray<FListedChannelInfo>();
	return EmptyResult;
}

const TMap<int32, FSubscribedChannelInfo> UChanneldGameInstanceSubsystem::GetSubscribedsOnOwnedChannel(bool& bSuccess, int32 ChId)
{
	bSuccess = false;
	if (ensureMsgf(ConnectionInstance, TEXT("Need to call ConnectToChanneld first!")))
	{
		FOwnedChannelInfo* OwnedChannel = ConnectionInstance->OwnedChannels.Find(ChId);
		if (OwnedChannel != nullptr)
		{
			bSuccess = true;
			return OwnedChannel->Subscribeds;
		}
	}
	const auto EmptyMap = TMap<int32, FSubscribedChannelInfo>();
	return EmptyMap;
}

FSubscribedChannelInfo UChanneldGameInstanceSubsystem::GetSubscribedOnOwnedChannelByConnId(bool& bSuccess, int32 ChId, int32 ConnId)
{
	bSuccess = false;
	if (ensureMsgf(ConnectionInstance, TEXT("Need to call ConnectToChanneld first!")))
	{
		FOwnedChannelInfo* OwnedChannel = ConnectionInstance->OwnedChannels.Find(ChId);
		if (OwnedChannel != nullptr)
		{
			FSubscribedChannelInfo* SubscribedInfo = OwnedChannel->Subscribeds.Find(ConnId);
			if (SubscribedInfo != nullptr)
			{
				bSuccess = true;
				return *SubscribedInfo;
			}
		}
	}
	return FSubscribedChannelInfo();
}

void UChanneldGameInstanceSubsystem::ConnectToChanneld(bool& Success, FString& Error, FString Host, int32 Port, const FOnceOnAuth& AuthCallback, bool bInitAsClient)
{
	Success = false;
	
	InitConnection();

	if (ConnectionInstance->IsConnected())
	{
		Error = TEXT("Already connected to channeld");
		return;
	}

	if (ConnectionInstance->Connect(bInitAsClient, Host, Port, Error))
	{
		ConnectionInstance->Auth(TEXT("test_pit"), TEXT("test_lt"),
			[AuthCallback](const channeldpb::AuthResultMessage* Message)
			{
				AuthCallback.ExecuteIfBound(Message->result(), Message->connid());
			});
		Success = true;
	}
	else
	{
		Error = TEXT("Failed to connect to channeld");
	}
}

void UChanneldGameInstanceSubsystem::ConnectWithDefaultSettings(bool& Success, FString& Error, const FOnceOnAuth& AuthCallback, bool bInitAsClient)
{
	FString Host;
	int Port;
	auto Settings = GetMutableDefault<UChanneldSettings>();
	if (bInitAsClient)
	{
		Host = Settings->ChanneldIpForClient;
		Port = Settings->ChanneldPortForClient;
	}
	else
	{
		Host = Settings->ChanneldIpForServer;
		Port = Settings->ChanneldPortForServer;
	}

	ConnectToChanneld(Success, Error, Host, Port, AuthCallback, bInitAsClient);
}

void UChanneldGameInstanceSubsystem::DisconnectFromChanneld(bool bFlushAll/* = true*/)
{
	if (ConnectionInstance)
	{
		ConnectionInstance->Disconnect(bFlushAll);
	}
}

void UChanneldGameInstanceSubsystem::CreateChannel(EChanneldChannelType ChannelType, FString Metadata, UProtoMessageObject* InitData,
	const FOnceOnCreateChannel& Callback)
{
	InitConnection();

	ConnectionInstance->CreateChannel(
		static_cast<channeldpb::ChannelType>(ChannelType),
		Metadata,
		nullptr,
		InitData == nullptr ? nullptr : InitData->GetMessage(),
		nullptr,
		[=](const channeldpb::CreateChannelResultMessage* Message)
		{
			Callback.ExecuteIfBound(Message->channelid(), static_cast<EChanneldChannelType>(Message->channeltype()), FString(UTF8_TO_TCHAR(Message->metadata().c_str())), Message->ownerconnid());
		}
	);
}

void UChanneldGameInstanceSubsystem::RemoveChannel(int32 ChannelToRemove, const FOnceOnRemoveChannel& Callback)
{
	InitConnection();

	ConnectionInstance->RemoveChannel(ChannelToRemove,
		[=](const channeldpb::RemoveChannelMessage* Message)
		{
			Callback.ExecuteIfBound(0, Message->channelid());
		}
	);
}

void UChanneldGameInstanceSubsystem::ListChannel(EChanneldChannelType ChannelTypeFilter, const TArray<FString>& MetadataFilters, const FOnceOnListChannel& Callback)
{
	InitConnection();

	ConnectionInstance->ListChannel(static_cast<channeldpb::ChannelType>(ChannelTypeFilter),
		&MetadataFilters, [=](const channeldpb::ListChannelResultMessage* Message)
		{
			TArray<FListedChannelInfo> Channels;
			ConnectionInstance->ListedChannels.GenerateValueArray(Channels);
			Callback.ExecuteIfBound(Channels);
		}
	);
}

void UChanneldGameInstanceSubsystem::SubToChannel(int32 ChId, bool bHasSubOptions, const FChannelSubscriptionOptions& SubOptions, const FOnceOnSubToChannel& Callback)
{
	InitConnection();

	SubConnectionToChannel(ConnectionInstance->GetConnId(), ChId, bHasSubOptions, SubOptions, Callback);
}

void UChanneldGameInstanceSubsystem::SubConnectionToChannel(int32 TargetConnId, int32 ChId, bool bHasSubOptions, const FChannelSubscriptionOptions& SubOptions, const FOnceOnSubToChannel& Callback)
{
	InitConnection();

	ConnectionInstance->SubConnectionToChannel(TargetConnId, ChId, bHasSubOptions ? SubOptions.ToMessage().Get() : nullptr,
		[=](const channeldpb::SubscribedToChannelResultMessage* Message)
		{
			Callback.ExecuteIfBound(ChId, static_cast<EChanneldChannelType>(Message->channeltype()), Message->connid(), static_cast<EChanneldConnectionType>(Message->conntype()));
		}
	);
}

void UChanneldGameInstanceSubsystem::UnsubFromChannel(int32 ChId, const FOnceOnUnsubFromChannel& Callback)
{
	InitConnection();

	UnsubConnectionFromChannel(ConnectionInstance->GetConnId(), ChId, Callback);
}

void UChanneldGameInstanceSubsystem::UnsubConnectionFromChannel(int32 TargetConnId, int32 ChId, const FOnceOnUnsubFromChannel& Callback)
{
	InitConnection();

	ConnectionInstance->UnsubConnectionFromChannel(TargetConnId, ChId,
		[=](const channeldpb::UnsubscribedFromChannelResultMessage* Message)
		{
			Callback.ExecuteIfBound(ChId, static_cast<EChanneldChannelType>(Message->channeltype()), Message->connid(), static_cast<EChanneldConnectionType>(Message->conntype()));
		}
	);
}

void UChanneldGameInstanceSubsystem::SendDataUpdate(int32 ChId, UProtoMessageObject* MessageObject)
{
	InitConnection();

	channeldpb::ChannelDataUpdateMessage UpdateMsg;
	UpdateMsg.mutable_data()->PackFrom(*MessageObject->GetMessage());
	ConnectionInstance->Send(ChId, channeldpb::CHANNEL_DATA_UPDATE, UpdateMsg);
}

void UChanneldGameInstanceSubsystem::QuerySpatialChannel(const AActor* Actor,
	const FOnceOnQuerySpatialChannel& Callback)
{
	InitConnection();

	TArray<FVector> Positions;
	Positions.Add(Actor->GetActorLocation());
	ConnectionInstance->QuerySpatialChannel(Positions, [Callback](const channeldpb::QuerySpatialChannelResultMessage* ResultMsg)
	{
		ChannelId ChId = ResultMsg->channelid_size() == 0 ? InvalidChannelId : ResultMsg->channelid(0);
		Callback.ExecuteIfBound((int64)ChId);
	});
}

void UChanneldGameInstanceSubsystem::ServerBroadcast(int32 ChId, int32 ClientConnId, UProtoMessageObject* MessageObject,
                                                     EChanneldBroadcastType BroadcastType)
{
	InitConnection();

	if (ConnectionInstance->IsClient())
	{
		return;
	}
	google::protobuf::Message* PayloadMessage = MessageObject->GetMessage();
	google::protobuf::Any AnyData;
	AnyData.PackFrom(*PayloadMessage);
	uint8* MessageData = new uint8[AnyData.ByteSizeLong()];
	bool Serialized = AnyData.SerializeToArray(MessageData, AnyData.GetCachedSize());
	if (!Serialized)
	{
		delete[] MessageData;
		UE_LOG(LogChanneld, Error, TEXT("Failed to serialize broadcast payload, type: %d"), BroadcastType);
		return;
	}
	channeldpb::ServerForwardMessage MessageWrapper;
	MessageWrapper.set_clientconnid(ClientConnId);
	MessageWrapper.set_payload(MessageData, AnyData.GetCachedSize());
	ConnectionInstance->Send(ChId, unrealpb::ANY, MessageWrapper, static_cast<channeldpb::BroadcastType>(BroadcastType));
}

void UChanneldGameInstanceSubsystem::RegisterChannelTypeByFullName(EChanneldChannelType ChannelType, FString ProtobufFullName)
{
	ChannelTypeToProtoFullNameMapping.Add(ChannelType, ProtobufFullName);
}

void UChanneldGameInstanceSubsystem::CreateMessageObjectByChannelType(UProtoMessageObject*& MessageObject,
	bool& bSuccess, EChanneldChannelType ChannelType)
{
	FString ProtoFullName = ChannelTypeToProtoFullNameMapping.FindRef(ChannelType);
	MessageObject = NewObject<UProtoMessageObject>();
	CreateMessageObjectByFullName(MessageObject, bSuccess, ProtoFullName);
}

void UChanneldGameInstanceSubsystem::CreateMessageObjectByFullName(UProtoMessageObject*& MessageObject, bool& bSuccess, FString ProtobufFullName)
{
	google::protobuf::Message* Message = ChanneldUtils::CreateProtobufMessage(TCHAR_TO_UTF8(*ProtobufFullName));
	MessageObject = NewObject<UProtoMessageObject>();

	if (Message != nullptr)
	{
		MessageObject->SetMessagePtr(Message, true);
		bSuccess = true;
	}
	else
	{
		UE_LOG(LogChanneld, Error, TEXT("Failed to create Protobuf message by name: %s"), *ProtobufFullName);
		bSuccess = false;
	}
}

UChannelDataView* UChanneldGameInstanceSubsystem::GetChannelDataView()
{
	return ChannelDataView;
}

void UChanneldGameInstanceSubsystem::SetLowLevelSendToChannelId(int32 ChId)
{
	*LowLevelSendToChannelId = ChId;

	// At this moment, we can add the queued providers to the right channels.
	for (TWeakInterfacePtr<IChannelDataProvider>& Provider : UnregisteredDataProviders)
	{
		if (Provider.IsValid())
		{
			// ChannelDataView->AddProvider(ChId, Provider.Get());
			ChannelDataView->AddProviderToDefaultChannel(Provider.Get());
		}
	}
	UnregisteredDataProviders.Empty();

	OnSetLowLevelSendChannelId.Broadcast();
}

void UChanneldGameInstanceSubsystem::OpenLevel(FName LevelName, bool bAbsolute /*= true*/, FString Options /*= FString(TEXT(""))*/)
{
	UGameplayStatics::OpenLevel(this, LevelName, bAbsolute, Options);
}

void UChanneldGameInstanceSubsystem::OpenLevelByObjPtr(const TSoftObjectPtr<UWorld> Level, bool bAbsolute /*= true*/, FString Options /*= FString(TEXT(""))*/)
{
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Level, bAbsolute, Options);
}

void UChanneldGameInstanceSubsystem::SeamlessTravelToChannel(APlayerController* PlayerController, int32 ChId)
{
	InitConnection();

	if (!HasSubscribedToChannel(ChId))
	{
		ConnectionInstance->SubToChannel(ChId);
	}
	SetLowLevelSendToChannelId(ChId);

	UWorld* World = GetWorld();
	// Map name starts with '/Game/Maps'
	FString MapName = World->RemovePIEPrefix(World->URL.Map); //World->GetMapName());// 
	PlayerController->ClientTravel(FString::Printf(TEXT("127.0.0.1%s"), *MapName), ETravelType::TRAVEL_Relative, true);
}

void UChanneldGameInstanceSubsystem::HandleAuthResult(UChanneldConnection* Conn, ChannelId ChId,
	const google::protobuf::Message* Msg)
{
	auto AuthResultMsg = static_cast<const channeldpb::AuthResultMessage*>(Msg);

	if (AuthResultMsg->result() == channeldpb::AuthResultMessage_AuthResult_SUCCESSFUL && AuthResultMsg->connid() == Conn->GetConnId())
	{
		InitChannelDataView();
	}
	
	OnAuth.Broadcast(AuthResultMsg->result(), AuthResultMsg->connid());
}

void UChanneldGameInstanceSubsystem::HandleCreateChannel(UChanneldConnection* Conn, ChannelId ChId,
	const google::protobuf::Message* Msg)
{
	auto CreateResultMsg = static_cast<const channeldpb::CreateChannelResultMessage*>(Msg);
	OnCreateChannel.Broadcast(ChId, static_cast<EChanneldChannelType>(CreateResultMsg->channeltype()), FString(UTF8_TO_TCHAR(CreateResultMsg->metadata().c_str())), CreateResultMsg->ownerconnid());
}

void UChanneldGameInstanceSubsystem::HandleRemoveChannel(UChanneldConnection* Conn, ChannelId ChId,
	const google::protobuf::Message* Msg)
{
	auto RemoveResultMsg = static_cast<const channeldpb::RemoveChannelMessage*>(Msg);
	OnRemoveChannel.Broadcast(ChId, RemoveResultMsg->channelid());
}

void UChanneldGameInstanceSubsystem::HandleListChannel(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{
	TArray<FListedChannelInfo> Channels;
	Conn->ListedChannels.GenerateValueArray(Channels);
	OnListChannel.Broadcast(Channels);
}

void UChanneldGameInstanceSubsystem::HandleSubToChannel(UChanneldConnection* Conn, ChannelId ChId,
	const google::protobuf::Message* Msg)
{
	auto SubResultMsg = static_cast<const channeldpb::SubscribedToChannelResultMessage*>(Msg);
	OnSubToChannel.Broadcast(ChId, static_cast<EChanneldChannelType>(SubResultMsg->channeltype()), SubResultMsg->connid(), static_cast<EChanneldConnectionType>(SubResultMsg->conntype()));
}

void UChanneldGameInstanceSubsystem::HandleUnsubFromChannel(UChanneldConnection* Conn, ChannelId ChId,
	const google::protobuf::Message* Msg)
{
	auto UnsubResultMsg = static_cast<const channeldpb::UnsubscribedFromChannelResultMessage*>(Msg);
	OnUnsubFromChannel.Broadcast(ChId, static_cast<EChanneldChannelType>(UnsubResultMsg->channeltype()), UnsubResultMsg->connid(), static_cast<EChanneldConnectionType>(UnsubResultMsg->conntype()));
}

void UChanneldGameInstanceSubsystem::HandleChannelDataUpdate(UChanneldConnection* Conn, ChannelId ChId,
	const google::protobuf::Message* Msg)
{
	if (!OnDataUpdate.IsBound())
	{
		return;
	}
	auto UpdateResultMsg = static_cast<const channeldpb::ChannelDataUpdateMessage*>(Msg);
	const google::protobuf::Any& AnyData = UpdateResultMsg->data();

	UProtoMessageObject* MessageObject = NewObject<UProtoMessageObject>();
	bool Success;
	CreateMessageObjectByChannelType(MessageObject, Success, GetChannelTypeByChId(ChId));
	if (ensureMsgf((Success && MessageObject->GetMessage() != nullptr), TEXT("ChannelType: %d is not registered. Please register ChannelType befor recive data update by function: RegisterChannelTypeByFullName"), GetChannelTypeByChId(ChId)))
	{
		AnyData.UnpackTo(MessageObject->GetMessage());
		OnDataUpdate.Broadcast(ChId, GetChannelTypeByChId(ChId), MessageObject, UpdateResultMsg->contextconnid());
	}
}

void UChanneldGameInstanceSubsystem::HandleUserSpaceAnyMessage(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto AnyMsg = static_cast<const google::protobuf::Any*>(Msg);
	std::string ProtoFullName = AnyMsg->type_url();
	if (ProtoFullName.length() < 20)
	{
		UE_LOG(LogChanneld, Warning, TEXT("The typeURL in the user-space Any message is too short: %s"), UTF8_TO_TCHAR(ProtoFullName.c_str()));
		return;
	}

	// Erase "type.googleapis.com/"
	ProtoFullName.erase(0, 20);
	auto PayloadMsg = ChanneldUtils::CreateProtobufMessage(ProtoFullName);
	if (PayloadMsg != nullptr)
	{
		AnyMsg->UnpackTo(PayloadMsg);
		UProtoMessageObject* MsgObject = NewObject<UProtoMessageObject>();
		MsgObject->SetMessagePtr(PayloadMsg, true);
		OnUserSpaceMessage.Broadcast(ChId, Conn->GetConnId(), MsgObject);
	}
	else
	{
		UE_LOG(LogChanneld, Warning, TEXT("Couldn't find Protobuf message type by name: %s"), UTF8_TO_TCHAR(ProtoFullName.c_str()));
	}
}

UChanneldNetDriver* UChanneldGameInstanceSubsystem::GetNetDriver()
{
	//auto NetDriver = GetGameInstance()->GetWorld()->GetNetDriver();
	return Cast<UChanneldNetDriver>(GetGameInstance()->GetWorld()->GetNetDriver());
}

void UChanneldGameInstanceSubsystem::InitChannelDataView()
{
	TSubclassOf<UChannelDataView> ChannelDataViewClass = nullptr;

	ChannelDataViewClass = GetMutableDefault<UChanneldSettings>()->ChannelDataViewClass;

	if (ChannelDataViewClass)
	{
		ChannelDataView = NewObject<UChannelDataView>(this, ChannelDataViewClass);
		//ConnToChanneld->OnAuthenticated.AddUObject(ChannelDataView, &UChannelDataView::Initialize);
		ChannelDataView->Initialize(ConnectionInstance);

		OnViewInitialized.Broadcast(ChannelDataView);
	}
	else
	{
		UE_LOG(LogChanneld, Error, TEXT("Failed to load any ChannelDataView"));
	}
}

void UChanneldGameInstanceSubsystem::RegisterDataProvider(IChannelDataProvider* Provider)
{
	if (ChannelDataView)
	{
		/* DO NOT use LowLevelSendToChannelId here as it's for LowLevelSend() only!
		ChannelDataView->AddProvider(LowLevelSendToChannelId.Get(), Provider);
		*/
		ChannelDataView->AddProviderToDefaultChannel(Provider);
	}
	else
	{
		// If the view is not initialized yet, postpone the registration to SetLowLevelSendToChannelId()
		UnregisteredDataProviders.Add(Provider);
	}
}
