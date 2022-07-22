// Fill out your copyright notice in the Description page of Project Settings.


#include "ProtoMessageObject.h"
#include "ChanneldConnection.h"
#include "google/protobuf/any.pb.h"

UProtoMessageObject::~UProtoMessageObject()
{
	if (bMessageLifeTogether)
	{
		delete Message;
		Message = nullptr;
	}
}

google::protobuf::Message* UProtoMessageObject::GetMessage()
{
	return Message;
}

void UProtoMessageObject::SetMessage(const google::protobuf::Message* Msg)
{
	bMessageLifeTogether = true;
	Message = Msg->New();
	Message->CopyFrom(*Msg);
}

void UProtoMessageObject::SetMessagePtr(const google::protobuf::Message* Msg, bool bLifeTogether = false)
{
	if (bLifeTogether)
	{
		this->bMessageLifeTogether = true;
	}
	Message = const_cast<google::protobuf::Message*>(Msg);
}

FString UProtoMessageObject::GetMessageDebugInfo()
{
	return FString(Message->DebugString().c_str());
}

void UProtoMessageObject::Clear()
{
	Message->Clear();
}

UProtoMessageObject* UProtoMessageObject::Clone()
{
	UProtoMessageObject* NewProtoMessageObject = NewObject<UProtoMessageObject>();
	NewProtoMessageObject->SetMessage(Message);
	return NewProtoMessageObject;
}

UProtoMessageObject* UProtoMessageObject::CloneEmpty()
{
	UProtoMessageObject* NewProtoMessageObject = NewObject<UProtoMessageObject>();
	NewProtoMessageObject->SetMessagePtr(Message->New(), true);
	return NewProtoMessageObject;
}

UProtoMessageObject* UProtoMessageObject::CloneEmptyFieldMessage(bool& bSuccess, FString FieldName)
{
	return GetMessageByName(bSuccess, FieldName)->CloneEmpty();
}

int32 UProtoMessageObject::GetInt32ByName(bool& bSuccess, FString FieldName)
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

int64 UProtoMessageObject::GetInt64ByName(bool& bSuccess, FString FieldName)
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

float UProtoMessageObject::GetFloatByName(bool& bSuccess, FString FieldName)
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

FString UProtoMessageObject::GetStringByName(bool& bSuccess, FString FieldName)
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

UProtoMessageObject* UProtoMessageObject::GetMessageByName(bool& bSuccess, FString FieldName)
{
	const google::protobuf::FieldDescriptor* FD = Message->GetDescriptor()->FindFieldByName(TCHAR_TO_UTF8(*FieldName));
	UProtoMessageObject* ChildMessage = NewObject<UProtoMessageObject>();
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

void UProtoMessageObject::UnpackMessageByFullProtoName(bool& bSuccess, UProtoMessageObject*& UnpackedMsg, 	FString ProtoName)
{
	UnpackedMsg = NewObject<UProtoMessageObject>();
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

void UProtoMessageObject::GetMessagesRepeatedByName(bool& bSuccess, TArray<UProtoMessageObject*>& MessageRepeated, FString FieldName)
{
	const google::protobuf::FieldDescriptor* FD = Message->GetDescriptor()->FindFieldByName(TCHAR_TO_UTF8(*FieldName));
	if (FD != NULL)
	{
		int Size = Message->GetReflection()->FieldSize(*Message, FD);
		for (int i = 0; i < Size; i++)
		{
			const google::protobuf::Message& ChildMessageRef = Message->GetReflection()->GetRepeatedMessage(*Message, FD, i);
			UProtoMessageObject* Msg = NewObject<UProtoMessageObject>();
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

UProtoMessageObject* UProtoMessageObject::SetInt32ByName(bool& bSuccess, FString FieldName, int32 Value)
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

UProtoMessageObject* UProtoMessageObject::SetInt64ByName(bool& bSuccess, FString FieldName, int64 Value)
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

UProtoMessageObject* UProtoMessageObject::SetFloatByName(bool& bSuccess, FString FieldName, float Value)
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

UProtoMessageObject* UProtoMessageObject::SetStringByName(bool& bSuccess, FString FieldName, FString Value)
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

UProtoMessageObject* UProtoMessageObject::AddMessageToRepeatedField(bool& bSuccess, FString FieldName, UProtoMessageObject* ChildMessage)
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
