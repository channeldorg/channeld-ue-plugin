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

ECDChannelType UChanneldGameInstanceSubsystem::GetChannelTypeByChId(int32 ChId)
{
	return static_cast<ECDChannelType>(GetProtoChannelTypeByChId(ChId));
}

channeldpb::ChannelType UChanneldGameInstanceSubsystem::GetProtoChannelTypeByChId(int32 ChId)
{
	auto SubChannelResMsg = ConnectionInstance->SubscribedChannels.FindRef(ChId);
	if (SubChannelResMsg != nullptr)
	{
		return SubChannelResMsg->channeltype();
	}
	return channeldpb::ChannelType::UNKNOWN;
}

void UChanneldGameInstanceSubsystem::ConnectToChanneld(bool& Success, FString& Error, FString Host, int32 Port, const FOnceOnAuth& AuthCallback)
{
	Success = false;
	if (ConnectionInstance->Connect(true, Host, Port, Error))
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

void UChanneldGameInstanceSubsystem::CreateChannel(ECDChannelType ChannelType, FString Metadata, UProtoMessageObject* InitData,
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
			UE_LOG(LogTemp, Warning, TEXT("channelid: %d   channeltype: %d   metadata: %s   ownerconnid: %d "), Message->channelid(), Message->channeltype(), Message->metadata().c_str(), Message->ownerconnid());
			Callback.ExecuteIfBound(Message->channelid(), static_cast<ECDChannelType>(Message->channeltype()), FString(Message->metadata().c_str()), Message->ownerconnid());
		}
	);
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
			Callback.ExecuteIfBound(ChId, static_cast<ECDChannelType>(Message->channeltype()), Message->connid(), Message->conntype());
		}
	);
}

void UChanneldGameInstanceSubsystem::SendDataUpdate(int32 ChId, UProtoMessageObject* MessageObject)
{
	channeldpb::ChannelDataUpdateMessage UpdateMsg;
	UpdateMsg.mutable_data()->PackFrom(*MessageObject->GetMessage());
	ConnectionInstance->Send(ChId, channeldpb::CHANNEL_DATA_UPDATE, UpdateMsg);
}

bool UChanneldGameInstanceSubsystem::RegisterChannelTypeByFullName(ECDChannelType ChannelType, FString ProtobufFullName)
{
	const google::protobuf::Descriptor* Desc = google::protobuf::DescriptorPool::generated_pool()
		->FindMessageTypeByName(TCHAR_TO_UTF8(*ProtobufFullName));
	if (Desc)
	{
		google::protobuf::MessageFactory::generated_factory()->GetPrototype(Desc);
		ChannelTypeToMsgPrototypeMapping.Add(
			static_cast<channeldpb::ChannelType>(ChannelType),
			google::protobuf::MessageFactory::generated_factory()->GetPrototype(Desc)
		);
		return true;
	}
	return false;
}

void UChanneldGameInstanceSubsystem::CreateMessageObjectByChannelType(UProtoMessageObject*& MessageObject,
	bool& bSuccess, ECDChannelType ChannelType)
{
	const google::protobuf::Message* MsgPrototype = ChannelTypeToMsgPrototypeMapping.FindRef(static_cast<channeldpb::ChannelType>(ChannelType));
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
	OnCreateChannel.Broadcast(ChId, static_cast<ECDChannelType>(CreateResultMsg->channeltype()), FString(CreateResultMsg->metadata().c_str()), CreateResultMsg->ownerconnid());
}

void UChanneldGameInstanceSubsystem::HandleSubToChannel(UChanneldConnection* Conn, ChannelId ChId,
	const google::protobuf::Message* Msg)
{
	auto SubResultMsg = static_cast<const channeldpb::SubscribedToChannelResultMessage*>(Msg);
	OnSubToChannel.Broadcast(ChId, static_cast<ECDChannelType>(SubResultMsg->channeltype()), SubResultMsg->connid(), SubResultMsg->conntype());
}

void UChanneldGameInstanceSubsystem::HandleChannelDataUpdate(UChanneldConnection* Conn, ChannelId ChId,
	const google::protobuf::Message* Msg)
{
	auto UpdateResultMsg = static_cast<const channeldpb::ChannelDataUpdateMessage*>(Msg);
	const google::protobuf::Any& AnyData = UpdateResultMsg->data();

	const google::protobuf::Message* DataMsgPrototype = ChannelTypeToMsgPrototypeMapping.FindRef(GetProtoChannelTypeByChId(ChId));
	if (ensureMsgf(DataMsgPrototype != nullptr, TEXT("ChannelType: %d has no corresponding prototype. Please register message prototype befor recive data update by function: RegisterChannelTypeByFullName"), GetProtoChannelTypeByChId(ChId)))
	{
		google::protobuf::Message* DataMsg = DataMsgPrototype->New();
		AnyData.UnpackTo(DataMsg);

		UProtoMessageObject* MessageObject = NewObject<UProtoMessageObject>();
		MessageObject->SetMessagePtr(DataMsg, true);

		OnDataUpdate.Broadcast(ChId, GetChannelTypeByChId(ChId), MessageObject, UpdateResultMsg->contextconnid());
	}
}
