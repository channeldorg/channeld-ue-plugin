// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "google/protobuf/message.h"
#include "ProtoMessageObject.generated.h"

class UMessageRepeatedWrapper;

UCLASS(BlueprintType)
class UProtoMessageObject : public UObject
{
	GENERATED_BODY()
public:
	~UProtoMessageObject();

	google::protobuf::Message* GetMessage();

	void SetMessage(const google::protobuf::Message* Msg);
	void SetMessagePtr(const google::protobuf::Message* Msg, bool bLifeTogether/* = false*/);

	UFUNCTION(BlueprintCallable, BlueprintPure)
		FString GetMessageDebugInfo();

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf")
		void Clear();

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf")
		UProtoMessageObject* Clone();

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf")
		UProtoMessageObject* CloneEmpty();

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf")
		UProtoMessageObject* CloneEmptyFieldMessage(bool& bSuccess, FString FieldName);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf", DisplayName = "Get Int32 By Name", BlueprintPure)
		int32 GetInt32ByName(bool& bSuccess, FString FieldName);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf", DisplayName = "Get Int64 By Name", BlueprintPure)
		int64 GetInt64ByName(bool& bSuccess, FString FieldName);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf", BlueprintPure)
		float GetFloatByName(bool& bSuccess, FString FieldName);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf", BlueprintPure)
		FString GetStringByName(bool& bSuccess, FString FieldName);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf", BlueprintPure)
		UProtoMessageObject* GetMessageByName(bool& bSuccess, FString FieldName);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf", BlueprintPure)
		void UnpackMessageByFullProtoName(bool& bSuccess, UProtoMessageObject*& UnpackedMsg, FString ProtoName);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf", BlueprintPure)
		void  GetMessagesRepeatedByName(bool& bSuccess, TArray<UProtoMessageObject*>& Messages, FString FieldName);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf", DisplayName = "Set Int32 By Name")
		UProtoMessageObject* SetInt32ByName(bool& bSuccess, FString FieldName, int32 Value);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf", DisplayName = "Set Int64 By Name")
		UProtoMessageObject* SetInt64ByName(bool& bSuccess, FString FieldName, int64 Value);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf")
		UProtoMessageObject* SetFloatByName(bool& bSuccess, FString FieldName, float Value);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf")
		UProtoMessageObject* SetStringByName(bool& bSuccess, FString FieldName, FString Value);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf")
		UProtoMessageObject* AddMessageToRepeatedField(bool& bSuccess, FString FieldName, UProtoMessageObject* ChildMessage);

protected:

	// The protobuf message
	google::protobuf::Message* Message;

	// If set true, the protobuf message will be deleted when ProtoMessageObject is destructing
	bool bMessageLifeTogether = false;
};
