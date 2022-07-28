// Fill out your copyright notice in the Description page of Project Settings.


#include "ChanneldGameInstanceSubsystem.h"
#include "ChanneldConnection.h"
#include "ProtoMessageObject.h"

void UChanneldGameInstanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	ConnectionInstance = NewObject<UChanneldConnection>();

	ConnectionInstance->AddMessageHandler((uint32)channeldpb::AUTH, this, &UChanneldGameInstanceSubsystem::HandleAuthResult);
	ConnectionInstance->AddMessageHandler((uint32)channeldpb::CREATE_CHANNEL, this, &UChanneldGameInstanceSubsystem::HandleCreateChannel);
	ConnectionInstance->AddMessageHandler((uint32)channeldpb::SUB_TO_CHANNEL, this, &UChanneldGameInstanceSubsystem::HandleSubToChannel);
	ConnectionInstance->AddMessageHandler((uint32)channeldpb::UNSUB_FROM_CHANNEL, this, &UChanneldGameInstanceSubsystem::HandleUnsubFromChannel);
	ConnectionInstance->AddMessageHandler((uint32)channeldpb::CHANNEL_DATA_UPDATE, this, &UChanneldGameInstanceSubsystem::HandleChannelDataUpdate);
}

void UChanneldGameInstanceSubsystem::Deinitialize()
{
	ConnectionInstance->Disconnect();
}

void UChanneldGameInstanceSubsystem::Tick(float DeltaTime)
{
	if (ConnectionInstance && ConnectionInstance->IsConnected())
	{
		ConnectionInstance->TickIncoming();
		ConnectionInstance->TickOutgoing();
	}
}

bool UChanneldGameInstanceSubsystem::IsConnected()
{
	return ConnectionInstance->IsConnected();
}

int32 UChanneldGameInstanceSubsystem::GetConnId()
{
	return ConnectionInstance->GetConnId();
}

EChanneldChannelType UChanneldGameInstanceSubsystem::GetChannelTypeByChId(int32 ChId)
{
	FSubscribedChannelInfo* SubscribedChannelInfo = ConnectionInstance->SubscribedChannels.Find(ChId);
	return SubscribedChannelInfo != nullptr ? SubscribedChannelInfo->ChannelType : EChanneldChannelType::ECT_Unknown;
}

TMap<int32, FSubscribedChannelInfo> UChanneldGameInstanceSubsystem::GetSubscribedsOnOwnedChannel(bool& bSuccess, int32 ChId)
{
	bSuccess = false;
	FOwnedChannelInfo* OwnedChannel = ConnectionInstance->OwnedChannels.Find(ChId);
	if (OwnedChannel != nullptr)
	{
		bSuccess = true;
		return OwnedChannel->Subscribeds;
	}
	return TMap<int32, FSubscribedChannelInfo>();
}

FSubscribedChannelInfo UChanneldGameInstanceSubsystem::GetSubscribedOnOwnedChannelByConnId(bool& bSuccess, int32 ChId, int32 ConnId)
{
	bSuccess = false;
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
	return FSubscribedChannelInfo();
}

void UChanneldGameInstanceSubsystem::ConnectToChanneld(bool& Success, FString& Error, FString Host, int32 Port, const FOnceOnAuth& AuthCallback, bool bInitAsClient)
{
	Success = false;
	if (ConnectionInstance->Connect(bInitAsClient, Host, Port, Error))
	{
		ConnectionInstance->Auth(TEXT("test_pit"), TEXT("test_lt"),
			[=](const channeldpb::AuthResultMessage* Message)
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

void UChanneldGameInstanceSubsystem::DisconnectFromChanneld(bool bFlushAll/* = true*/)
{
	ConnectionInstance->Disconnect(bFlushAll);
}

void UChanneldGameInstanceSubsystem::CreateChannel(EChanneldChannelType ChannelType, FString Metadata, UProtoMessageObject* InitData,
	const FOnceOnCreateChannel& Callback)
{
	ConnectionInstance->CreateChannel(
		static_cast<channeldpb::ChannelType>(ChannelType),
		Metadata,
		nullptr,
		InitData == nullptr ? nullptr : InitData->GetMessage(),
		nullptr,
		[=](const channeldpb::CreateChannelResultMessage* Message)
		{
			Callback.ExecuteIfBound(Message->channelid(), static_cast<EChanneldChannelType>(Message->channeltype()), FString(Message->metadata().c_str()), Message->ownerconnid());
		}
	);
}

void UChanneldGameInstanceSubsystem::RemoveChannel(int32 ChId)
{
	ConnectionInstance->RemoveChannel(ChId);
}

void UChanneldGameInstanceSubsystem::SubToChannel(int32 ChId, const FOnceOnSubToChannel& Callback)
{
	SubConnectionToChannel(ConnectionInstance->GetConnId(), ChId, Callback);
}

void UChanneldGameInstanceSubsystem::SubConnectionToChannel(int32 TargetConnId, int32 ChId, const FOnceOnSubToChannel& Callback)
{
	ConnectionInstance->SubConnectionToChannel(TargetConnId, ChId, nullptr,
		[=](const channeldpb::SubscribedToChannelResultMessage* Message)
		{
			Callback.ExecuteIfBound(ChId, static_cast<EChanneldChannelType>(Message->channeltype()), Message->connid(), static_cast<EChanneldConnectionType>(Message->conntype()));
		}
	);
}

void UChanneldGameInstanceSubsystem::SendDataUpdate(int32 ChId, UProtoMessageObject* MessageObject)
{
	channeldpb::ChannelDataUpdateMessage UpdateMsg;
	UpdateMsg.mutable_data()->PackFrom(*MessageObject->GetMessage());
	ConnectionInstance->Send(ChId, channeldpb::CHANNEL_DATA_UPDATE, UpdateMsg);
}

void UChanneldGameInstanceSubsystem::Broadcast(int32 ChId, int32 ClientConnId, UProtoMessageObject* MessageObject,
	EChanneldBroadcastType BroadcastType)
{
	google::protobuf::Message* PayloadMessage = MessageObject->GetMessage();
	uint8* MessageData = new uint8[PayloadMessage->ByteSizeLong()];
	bool Serialized = PayloadMessage->SerializeToArray(MessageData, PayloadMessage->GetCachedSize());
	if (!Serialized)
	{
		delete[] MessageData;
		UE_LOG(LogChanneld, Error, TEXT("Failed to serialize broadcast payload, type: %d"), BroadcastType);
		return;
	}
	channeldpb::ServerForwardMessage MessageWrapper;
	MessageWrapper.set_clientconnid(ClientConnId);
	MessageWrapper.set_payload(MessageData, PayloadMessage->GetCachedSize());
	ConnectionInstance->Send(ChId, 101, MessageWrapper, static_cast<channeldpb::BroadcastType>(BroadcastType));
}

bool UChanneldGameInstanceSubsystem::RegisterChannelTypeByFullName(EChanneldChannelType ChannelType, FString ProtobufFullName)
{
	const google::protobuf::Descriptor* Desc = google::protobuf::DescriptorPool::generated_pool()
		->FindMessageTypeByName(TCHAR_TO_UTF8(*ProtobufFullName));
	if (Desc)
	{
		google::protobuf::MessageFactory::generated_factory()->GetPrototype(Desc);
		ChannelTypeToMsgPrototypeMapping.Add(
			ChannelType,
			google::protobuf::MessageFactory::generated_factory()->GetPrototype(Desc)
		);
		return true;
	}
	return false;
}

void UChanneldGameInstanceSubsystem::CreateMessageObjectByChannelType(UProtoMessageObject*& MessageObject,
	bool& bSuccess, EChanneldChannelType ChannelType)
{
	const google::protobuf::Message* MsgPrototype = ChannelTypeToMsgPrototypeMapping.FindRef(ChannelType);
	MessageObject = NewObject<UProtoMessageObject>();
	bSuccess = false;
	if (MsgPrototype != nullptr)
	{
		MessageObject->SetMessage(MsgPrototype);
	}
	bSuccess = false;
}

void UChanneldGameInstanceSubsystem::CreateMessageObjectByFullName(UProtoMessageObject*& MessageObject, bool& bSuccess, FString ProtobufFullName)
{
	const google::protobuf::Descriptor* Desc = google::protobuf::DescriptorPool::generated_pool()
		->FindMessageTypeByName(TCHAR_TO_UTF8(*ProtobufFullName));
	google::protobuf::Message* Message = nullptr;
	MessageObject = NewObject<UProtoMessageObject>();
	bSuccess = false;
	if (ensure(Desc != nullptr))
	{
		Message = google::protobuf::MessageFactory::generated_factory()
			->GetPrototype(Desc)->New();
	}
	else
	{
		UE_LOG(LogChanneld, Error, TEXT("No protoType in DescriptorPool: %s. May not include xxx.pb.h file or override MakeSureCompilePB"), *ProtobufFullName);
	}
	if (ensure(Message != nullptr))
	{
		MessageObject->SetMessagePtr(Message, true);
		bSuccess = true;
	}
}

void UChanneldGameInstanceSubsystem::HandleAuthResult(UChanneldConnection* Conn, ChannelId ChId,
	const google::protobuf::Message* Msg)
{
	auto AuthResultMsg = static_cast<const channeldpb::AuthResultMessage*>(Msg);
	OnAuth.Broadcast(AuthResultMsg->result(), AuthResultMsg->connid());
}

void UChanneldGameInstanceSubsystem::HandleCreateChannel(UChanneldConnection* Conn, ChannelId ChId,
	const google::protobuf::Message* Msg)
{
	auto CreateResultMsg = static_cast<const channeldpb::CreateChannelResultMessage*>(Msg);
	OnCreateChannel.Broadcast(ChId, static_cast<EChanneldChannelType>(CreateResultMsg->channeltype()), FString(CreateResultMsg->metadata().c_str()), CreateResultMsg->ownerconnid());
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
	auto UpdateResultMsg = static_cast<const channeldpb::ChannelDataUpdateMessage*>(Msg);
	const google::protobuf::Any& AnyData = UpdateResultMsg->data();

	const google::protobuf::Message* DataMsgPrototype = ChannelTypeToMsgPrototypeMapping.FindRef(GetChannelTypeByChId(ChId));
	if (ensureMsgf(DataMsgPrototype != nullptr, TEXT("ChannelType: %d has no corresponding prototype. Please register message prototype befor recive data update by function: RegisterChannelTypeByFullName"), GetChannelTypeByChId(ChId)))
	{
		google::protobuf::Message* DataMsg = DataMsgPrototype->New();
		AnyData.UnpackTo(DataMsg);

		UProtoMessageObject* MessageObject = NewObject<UProtoMessageObject>();
		MessageObject->SetMessagePtr(DataMsg, true);

		OnDataUpdate.Broadcast(ChId, GetChannelTypeByChId(ChId), MessageObject, UpdateResultMsg->contextconnid());
	}
}
