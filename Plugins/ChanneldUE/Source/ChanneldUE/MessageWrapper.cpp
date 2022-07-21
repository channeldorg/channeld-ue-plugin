// Fill out your copyright notice in the Description page of Project Settings.


#include "MessageWrapper.h"
#include "ChanneldConnection.h"
#include "google/protobuf/any.pb.h"

UMessageWrapper::~UMessageWrapper()
{
	if (bMessageLifeTogether)
	{
		delete Message;
		Message = nullptr;
	}
}

google::protobuf::Message* UMessageWrapper::GetMessage()
{
	return Message;
}

void UMessageWrapper::SetMessage(const google::protobuf::Message* Msg)
{
	bMessageLifeTogether = true;
	Message = Msg->New();
	Message->CopyFrom(*Msg);
}

void UMessageWrapper::SetMessagePtr(const google::protobuf::Message* Msg, bool bLifeTogether = false)
{
	if (bLifeTogether)
	{
		this->bMessageLifeTogether = true;
	}
	Message = const_cast<google::protobuf::Message*>(Msg);
}

FString UMessageWrapper::GetMessageDebugInfo()
{
	return FString(Message->DebugString().c_str());
}

UMessageWrapper* UMessageWrapper::SpawnCopiedMessage()
{
	UMessageWrapper* NewMessageWrapper = NewObject<UMessageWrapper>();
	NewMessageWrapper->SetMessage(Message);
	return NewMessageWrapper;
}

UMessageWrapper* UMessageWrapper::SpawnEmptyMessage()
{
	UMessageWrapper* NewMessageWrapper = NewObject<UMessageWrapper>();
	NewMessageWrapper->SetMessagePtr(Message->New(), true);
	return NewMessageWrapper;
}

UMessageWrapper* UMessageWrapper::SpawnEmptyMessageByName(bool& bSuccess, FString FieldName)
{
	return GetMessageByName(bSuccess, FieldName)->SpawnEmptyMessage();
}

int32 UMessageWrapper::GetInt32ValueByName(bool& bSuccess, FString FieldName)
{
	const google::protobuf::FieldDescriptor* FD = Message->GetDescriptor()->FindFieldByName(TCHAR_TO_UTF8(*FieldName));
	if (FD != NULL)
	{
		bSuccess = true;
		return Message->GetReflection()->GetInt32(*Message, FD);
	}
	else
	{
		bSuccess = false;
		return 0;
	}
}

int64 UMessageWrapper::GetInt64ValueByName(bool& bSuccess, FString FieldName)
{
	const google::protobuf::FieldDescriptor* FD = Message->GetDescriptor()->FindFieldByName(TCHAR_TO_UTF8(*FieldName));
	if (FD != NULL)
	{
		bSuccess = true;
		return Message->GetReflection()->GetInt64(*Message, FD);
	}
	else
	{
		bSuccess = false;
		return 0;
	}
}

float UMessageWrapper::GetIntFloatValueByName(bool& bSuccess, FString FieldName)
{
	const google::protobuf::FieldDescriptor* FD = Message->GetDescriptor()->FindFieldByName(TCHAR_TO_UTF8(*FieldName));
	if (FD != NULL)
	{
		bSuccess = true;
		return Message->GetReflection()->GetFloat(*Message, FD);
	}
	else
	{
		bSuccess = false;
		return 0.f;
	}
}

FString UMessageWrapper::GetStringValueByName(bool& bSuccess, FString FieldName)
{
	const google::protobuf::FieldDescriptor* FD = Message->GetDescriptor()->FindFieldByName(TCHAR_TO_UTF8(*FieldName));
	if (FD != NULL)
	{
		bSuccess = true;
		return FString(UTF8_TO_TCHAR(Message->GetReflection()->GetString(*Message, FD).c_str()));
	}
	else
	{
		bSuccess = false;
		return FString();
	}
}

UMessageWrapper* UMessageWrapper::GetMessageByName(bool& bSuccess, FString FieldName)
{
	const google::protobuf::FieldDescriptor* FD = Message->GetDescriptor()->FindFieldByName(TCHAR_TO_UTF8(*FieldName));
	UMessageWrapper* ChildMessage = NewObject<UMessageWrapper>();
	if (FD != NULL)
	{
		const google::protobuf::Message& ChildMessageRef = Message->GetReflection()->GetMessage(*Message, FD);
		ChildMessage->SetMessagePtr(&ChildMessageRef);
		bSuccess = true;
	}
	else
	{
		bSuccess = false;
	}
	return ChildMessage;

}

void UMessageWrapper::UnpackMessageByProtoName(bool& bSuccess, UMessageWrapper*& UnpackedMsg, 	FString ProtoName)
{
	UnpackedMsg = NewObject<UMessageWrapper>();
	bSuccess = false;

	const google::protobuf::Any* TargetMsgAny = dynamic_cast<const google::protobuf::Any*>(Message);
	if (TargetMsgAny == nullptr)
	{
		UE_LOG(LogChanneld, Error, TEXT("The class of TargetMsg->Message does not inherit from Any "));
		return;
	}

	const google::protobuf::Descriptor* Desc = google::protobuf::DescriptorPool::generated_pool()
		->FindMessageTypeByName(TCHAR_TO_UTF8(*ProtoName));
	if (Desc == nullptr)
	{
		UE_LOG(LogChanneld, Error, TEXT("No protoType in DescriptorPool: %s. May not include xxx.pb.h file or override MakeSureCompilePB"), *ProtoName);
		return;
	}

	google::protobuf::Message* NewMessage
		= google::protobuf::MessageFactory::generated_factory()->GetPrototype(Desc)->New();
	if (NewMessage == nullptr)
	{
		return;
	}

	TargetMsgAny->UnpackTo(NewMessage);
	UnpackedMsg->SetMessagePtr(NewMessage, true);
	bSuccess = true;
}

void UMessageWrapper::GetMessagesRepeatedByName(bool& bSuccess, TArray<UMessageWrapper*>& MessageRepeated, FString FieldName)
{
	const google::protobuf::FieldDescriptor* FD = Message->GetDescriptor()->FindFieldByName(TCHAR_TO_UTF8(*FieldName));
	if (FD != NULL)
	{
		int Size = Message->GetReflection()->FieldSize(*Message, FD);
		for (int i = 0; i < Size; i++)
		{
			const google::protobuf::Message& ChildMessageRef = Message->GetReflection()->GetRepeatedMessage(*Message, FD, i);
			UMessageWrapper* Msg = NewObject<UMessageWrapper>();
			Msg->SetMessagePtr(&ChildMessageRef);
			MessageRepeated.Add(Msg);
		}
		bSuccess = true;
	}
	else
	{
		bSuccess = false;
	}
}

UMessageWrapper* UMessageWrapper::SetInt32ValueByName(bool& bSuccess, FString FieldName, int32 Value)
{
	const google::protobuf::FieldDescriptor* FD = Message->GetDescriptor()->FindFieldByName(TCHAR_TO_UTF8(*FieldName));
	if (FD != NULL)
	{
		bSuccess = true;
		Message->GetReflection()->SetInt32(Message, FD, Value);
	}
	else
	{
		bSuccess = false;
	}
	return this;
}

UMessageWrapper* UMessageWrapper::SetInt64ValueByName(bool& bSuccess, FString FieldName, int64 Value)
{
	const google::protobuf::FieldDescriptor* FD = Message->GetDescriptor()->FindFieldByName(TCHAR_TO_UTF8(*FieldName));
	if (FD != NULL)
	{
		bSuccess = true;
		Message->GetReflection()->SetInt64(Message, FD, Value);
	}
	else
	{
		bSuccess = false;
	}
	return this;
}

UMessageWrapper* UMessageWrapper::SetIntFloatValueByName(bool& bSuccess, FString FieldName, float Value)
{
	const google::protobuf::FieldDescriptor* FD = Message->GetDescriptor()->FindFieldByName(TCHAR_TO_UTF8(*FieldName));
	if (FD != NULL)
	{
		bSuccess = true;
		Message->GetReflection()->SetFloat(Message, FD, Value);
	}
	else
	{
		bSuccess = false;
	}
	return this;
}

UMessageWrapper* UMessageWrapper::SetStringValueByName(bool& bSuccess, FString FieldName, FString Value)
{
	const google::protobuf::FieldDescriptor* FD = Message->GetDescriptor()->FindFieldByName(TCHAR_TO_UTF8(*FieldName));
	if (FD != NULL)
	{
		bSuccess = true;
		Message->GetReflection()->SetString(Message, FD, TCHAR_TO_UTF8(*Value));
	}
	else
	{
		bSuccess = false;
	}
	return this;
}

UMessageWrapper* UMessageWrapper::AddMessageToRepeatedField(bool& bSuccess, FString FieldName, UMessageWrapper* ChildMessage)
{
	const google::protobuf::FieldDescriptor* FD = Message->GetDescriptor()->FindFieldByName(TCHAR_TO_UTF8(*FieldName));
	if (FD != NULL)
	{
		google::protobuf::Message* NewMessage = Message->GetReflection()->AddMessage(Message, FD);
		NewMessage->CopyFrom(*ChildMessage->Message);
		bSuccess = true;
	}
	else
	{
		bSuccess = false;
	}
	return this;
}
