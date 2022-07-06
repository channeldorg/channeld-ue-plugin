// Fill out your copyright notice in the Description page of Project Settings.


#include "TestProtoMerge.h"

#include "Net/UnrealNetwork.h"

UTestProtoMerge::UTestProtoMerge()
	: TestChannelDataChanged(false)
{
}

void UTestProtoMerge::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UTestProtoMerge, TestText);
	DOREPLIFETIME(UTestProtoMerge, TestNum);

}

void UTestProtoMerge::SetTestText(const FString& NewValue)
{
	TestText = NewValue;
	TestChannelData.set_text(TCHAR_TO_UTF8(*NewValue));
	TestChannelDataChanged = true;
}

void UTestProtoMerge::SetTestNum(const int& NewValue)
{
	TestNum = NewValue;
	TestChannelData.set_num(NewValue);
	TestChannelDataChanged = true;
}

void UTestProtoMerge::PostLoad()
{
	UObject::PostLoad();
	auto const NetDriver = static_cast<UChanneldNetDriver*>(GetWorld()->NetDriver);
	if (NetDriver != nullptr)
		NetDriver->RegisterChannelDataProvider(this);
}

bool UTestProtoMerge::UpdateChannelData(google::protobuf::Message* ChannelData)
{
	bool Result = TestChannelDataChanged;
	if (TestChannelDataChanged)
	{
		const auto TypedChannelData = static_cast<testpb::TestChannelDataMessage*>(ChannelData);
		TypedChannelData->MergeFrom(TestChannelData);
		TestChannelDataChanged = false;
	}
	return Result;
}

void UTestProtoMerge::OnChannelDataUpdated(const channeldpb::ChannelDataUpdateMessage* UpdateMsg)
{
	testpb::TestChannelDataMessage UpdateData;
	UpdateMsg->data().UnpackTo(&UpdateData);
	TestChannelData.MergeFrom(UpdateData);
}

//void UTestProtoMerge::Serialize(FArchive& Ar)
//{
//	UObject::Serialize(Ar);
//
//	const auto ByteString = TestChannelData.SerializeAsString();
//	UE_LOG(LogTemp, Log, TEXT("Serialized TestChannelData: %s"), ByteString.c_str());
//	TestChannelData.Clear();
//}
