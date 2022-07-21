// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "google/protobuf/message.h"
#include "MessageWrapper.generated.h"

class UMessageRepeatedWrapper;

UCLASS(BlueprintType)
class UMessageWrapper : public UObject
{
	GENERATED_BODY()
public:
	~UMessageWrapper();

	google::protobuf::Message* GetMessage();

	void SetMessage(const google::protobuf::Message* Msg);
	void SetMessagePtr(const google::protobuf::Message* Msg, bool bLifeTogether/* = false*/);

	UFUNCTION(BlueprintCallable, BlueprintPure)
		FString GetMessageDebugInfo();

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf")
		UMessageWrapper* SpawnCopiedMessage();
	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf")
		UMessageWrapper* SpawnEmptyMessage();
	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf")
		UMessageWrapper* SpawnEmptyMessageByName(bool& bSuccess, FString FieldName);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf", DisplayName = "Get Int32 Value By Name", BlueprintPure)
		int32 GetInt32ValueByName(bool& bSuccess, FString FieldName);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf", DisplayName = "Get Int64 Value By Name", BlueprintPure)
		int64 GetInt64ValueByName(bool& bSuccess, FString FieldName);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf", BlueprintPure)
		float GetIntFloatValueByName(bool& bSuccess, FString FieldName);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf", BlueprintPure)
		FString GetStringValueByName(bool& bSuccess, FString FieldName);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf", BlueprintPure)
		UMessageWrapper* GetMessageByName(bool& bSuccess, FString FieldName);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf", BlueprintPure)
		void UnpackMessageByProtoName(bool& bSuccess, UMessageWrapper*& UnpackedMsg, FString ProtoName);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf", BlueprintPure)
		void  GetMessagesRepeatedByName(bool& bSuccess, TArray<UMessageWrapper*>& Messages, FString FieldName);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf", DisplayName = "Set Int32 Value By Name")
		UMessageWrapper* SetInt32ValueByName(bool& bSuccess, FString FieldName, int32 Value);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf", DisplayName = "Set Int64 Value By Name")
		UMessageWrapper* SetInt64ValueByName(bool& bSuccess, FString FieldName, int64 Value);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf")
		UMessageWrapper* SetIntFloatValueByName(bool& bSuccess, FString FieldName, float Value);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf")
		UMessageWrapper* SetStringValueByName(bool& bSuccess, FString FieldName, FString Value);

	UFUNCTION(BlueprintCallable, Category = "Channeld|Protobuf")
		UMessageWrapper* AddMessageToRepeatedField(bool& bSuccess, FString FieldName, UMessageWrapper* ChildMessage);

protected:
	google::protobuf::Message* Message;
	bool bMessageLifeTogether = false;
};
