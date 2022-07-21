// Fill out your copyright notice in the Description page of Project Settings.


#include "ChanneldGameInstanceSubsystem.h"
#include "ChanneldConnection.h"
#include "MessageWrapper.h"

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

void UChanneldGameInstanceSubsystem::IsConnected(bool& bConnected)
{
	bConnected = ConnectionInstance->IsConnected();
}

void UChanneldGameInstanceSubsystem::GetConnId(int32& ConnId)
{
	ConnId = ConnectionInstance->GetConnId();
}

void UChanneldGameInstanceSubsystem::ConnectServer(bool& Success, FString& Error, FString Host, int32 Port, const FOnceOnAuth& AuthCallback)
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

void UChanneldGameInstanceSubsystem::CreateChannel(int32 ChannelType, FString Metadata, UMessageWrapper* InitData,
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
			Callback.ExecuteIfBound(Message->channelid(), Message->channeltype(), FString(Message->metadata().c_str()), Message->ownerconnid());
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
			Callback.ExecuteIfBound(ChId, Message->conntype(), Message->channeltype(), Message->connid());
		}
	);
}

void UChanneldGameInstanceSubsystem::SendDataUpdate(int32 ChId, UMessageWrapper* MsgWrapper)
{
	channeldpb::ChannelDataUpdateMessage UpdateMsg;
	UpdateMsg.mutable_data()->PackFrom(*MsgWrapper->GetMessage());
	ConnectionInstance->Send(ChId, channeldpb::CHANNEL_DATA_UPDATE, UpdateMsg);
}

void UChanneldGameInstanceSubsystem::SpawnMessageByProtoType(UMessageWrapper*& MsgWrapper, bool& bSuccess, FString ProtoName)
{
	const google::protobuf::Descriptor* Desc = google::protobuf::DescriptorPool::generated_pool()
		->FindMessageTypeByName(TCHAR_TO_UTF8(*ProtoName));
	google::protobuf::Message* Message = nullptr;
	MsgWrapper = NewObject<UMessageWrapper>();
	bSuccess = false;
	if (ensure(Desc != nullptr))
	{
		Message = google::protobuf::MessageFactory::generated_factory()
			->GetPrototype(Desc)->New();
	}
	else
	{
		UE_LOG(LogChanneld, Error, TEXT("No protoType in DescriptorPool: %s. May not include xxx.pb.h file or override MakeSureCompilePB"), *ProtoName);
	}
	if (ensure(Message != nullptr))
	{
		MsgWrapper->SetMessagePtr(Message, true);
		bSuccess = true;
	}
}

void UChanneldGameInstanceSubsystem::GetNowTimestamp(int64& Now)
{
	FDateTime Time = FDateTime::Now();
	Now = Time.ToUnixTimestamp();
}

void UChanneldGameInstanceSubsystem::TimestampToDateTime(FDateTime& DateTime, int64 Timestamp)
{
	DateTime = FDateTime::FromUnixTimestamp(Timestamp);
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
	OnCreateChannel.Broadcast(ChId, CreateResultMsg->channeltype(), FString(CreateResultMsg->metadata().c_str()), CreateResultMsg->ownerconnid());
}

void UChanneldGameInstanceSubsystem::HandleSubToChannel(UChanneldConnection* Conn, ChannelId ChId,
	const google::protobuf::Message* Msg)
{
	auto SubResultMsg = static_cast<const channeldpb::SubscribedToChannelResultMessage*>(Msg);
	OnSubToChannel.Broadcast(ChId, SubResultMsg->conntype(), SubResultMsg->channeltype(), SubResultMsg->connid());
}

void UChanneldGameInstanceSubsystem::HandleChannelDataUpdate(UChanneldConnection* Conn, ChannelId ChId,
	const google::protobuf::Message* Msg)
{
	auto updateResultMsg = static_cast<const channeldpb::ChannelDataUpdateMessage*>(Msg);
	UMessageWrapper* MsgWrapper = NewObject<UMessageWrapper>();
	MsgWrapper->SetMessage(updateResultMsg);
	OnDataUpdate.Broadcast(ChId, MsgWrapper, updateResultMsg->contextconnid());
}
