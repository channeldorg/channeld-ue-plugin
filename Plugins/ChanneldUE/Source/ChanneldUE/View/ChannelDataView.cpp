#include "ChannelDataView.h"
#include "ChanneldGameInstanceSubsystem.h"
#include "ChanneldNetDriver.h"
#include "ChanneldUtils.h"
#include "Metrics.h"

UChannelDataView::UChannelDataView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UChannelDataView::RegisterChannelDataType(EChanneldChannelType ChannelType, const FString& MessageFullName)
{
	auto Msg = ChanneldUtils::CreateProtobufMessage(TCHAR_TO_UTF8(*MessageFullName));
	if (Msg)
	{
		RegisterChannelDataTemplate(static_cast<channeldpb::ChannelType>(ChannelType), Msg);
	}
	else
	{
		UE_LOG(LogChanneld, Error, TEXT("Failed to register channel data type by name: %s"), *MessageFullName);
	}
}

void UChannelDataView::Initialize(UChanneldConnection* InConn)
{
	if (Connection == nullptr)
	{
		Connection = InConn;

		LoadCmdLineArgs();

		Connection->AddMessageHandler(channeldpb::CHANNEL_DATA_UPDATE, this, &UChannelDataView::HandleChannelDataUpdate);
		Connection->AddMessageHandler(channeldpb::UNSUB_FROM_CHANNEL, this, &UChannelDataView::HandleUnsub);
	}

	if (Connection->IsServer())
	{
		InitServer();
	}
	else if (Connection->IsClient())
	{
		InitClient();
	}
	else
	{
		UE_LOG(LogChanneld, Warning, TEXT("Invalid connection type: %s"), 
			UTF8_TO_TCHAR(channeldpb::ConnectionType_Name(Connection->GetConnectionType()).c_str()));
		return;
	}
	
	UE_LOG(LogChanneld, Log, TEXT("%s initialized channels."), *this->GetClass()->GetName());
}

void UChannelDataView::InitServer()
{
	ReceiveInitServer();
}

void UChannelDataView::InitClient()
{
	ReceiveInitClient();
}

void UChannelDataView::UninitServer()
{
	ReceiveUninitServer();
}

void UChannelDataView::UninitClient()
{
	ReceiveUninitClient();
}

void UChannelDataView::Unintialize()
{
	if (Connection != nullptr)
	{
		Connection->RemoveMessageHandler(channeldpb::CHANNEL_DATA_UPDATE, this);
	}
	else
	{
		return;
	}

	if (Connection->IsServer())
	{
		UninitServer();
	}
	else if (Connection->IsClient())
	{
		UninitClient();
	}
	else
	{
		UE_LOG(LogChanneld, Warning, TEXT("Invalid connection type: %s"),
			UTF8_TO_TCHAR(channeldpb::ConnectionType_Name(Connection->GetConnectionType()).c_str()));
		return;
	}

	UE_LOG(LogChanneld, Log, TEXT("%s uninitialized channels."), *this->GetClass()->GetName());
}

void UChannelDataView::BeginDestroy()
{
	Super::BeginDestroy();

	ReceivedUpdateDataInChannels.Empty();
	delete AnyForTypeUrl;
	for (auto& Pair : ChannelDataTemplatesByTypeUrl)
	{
		delete Pair.Value;
	}
	ChannelDataTemplatesByTypeUrl.Empty();
	ChannelDataProviders.Empty();
}

void UChannelDataView::AddProvider(ChannelId ChId, IChannelDataProvider* Provider)
{
	ensureMsgf(Provider->GetChannelType() != channeldpb::UNKNOWN, TEXT("Invalid channel type of data provider: %s"), *IChannelDataProvider::GetName(Provider));
	if (!ChannelDataTemplates.Contains(Provider->GetChannelType()))
	{
		RegisterChannelDataTemplate(Provider->GetChannelType(), Provider->GetChannelDataTemplate());
	}

	TSet<FProviderInternal>& Providers = ChannelDataProviders.FindOrAdd(ChId);
	Providers.Add(Provider);
	ChannelDataProviders[ChId] = Providers;

	UE_LOG(LogChanneld, Log, TEXT("Added channel data provider %s to channel %d"), *IChannelDataProvider::GetName(Provider), ChId);
}

void UChannelDataView::RemoveProvider(ChannelId ChId, IChannelDataProvider* Provider, bool bSendRemoved)
{
	ensureMsgf(Provider->GetChannelType() != channeldpb::UNKNOWN, TEXT("Invalid channel type of data provider: %s"), *IChannelDataProvider::GetName(Provider));
	TSet<FProviderInternal>* Providers = ChannelDataProviders.Find(ChId);
	if (Providers != nullptr)
	{
		UE_LOG(LogChanneld, Log, TEXT("Removing channel data provider %s from channel %d"), *IChannelDataProvider::GetName(Provider), ChId);
		
		if (bSendRemoved)
		{
			Provider->SetRemoved();

			// Collect the removed states immediately (before the provider gets destroyed completely)
			google::protobuf::Message* RemovedData = RemovedProvidersData.FindRef(ChId);
			if (!RemovedData)
			{
				auto MsgTemplate = ChannelDataTemplates.FindRef(Provider->GetChannelType());
				RemovedData = MsgTemplate->New();
				RemovedProvidersData.Add(ChId, RemovedData);
			}
			Provider->UpdateChannelData(RemovedData);
		}
		else
		{
			Providers->Remove(Provider);
		}
	}
}

void UChannelDataView::RemoveProviderFromAllChannels(IChannelDataProvider* Provider, bool bSendRemoved)
{
	if (Connection == nullptr)
	{
		UE_LOG(LogChanneld, Error, TEXT("Unable to call UChannelDataView::RemoveProviderFromAllChannels. The connection to channeld hasn't been set up yet and there's no subscription to any channel."));
		return;
	}

	ensureMsgf(Provider->GetChannelType() != channeldpb::UNKNOWN, TEXT("Invalid channel type of data provider: %s"), *IChannelDataProvider::GetName(Provider));
	for (auto& Pair : Connection->SubscribedChannels)
	{
		if (static_cast<channeldpb::ChannelType>(Pair.Value.ChannelType) ==Provider->GetChannelType())
		{
			RemoveProvider(Pair.Key, Provider, bSendRemoved);
			return;
		}
	}
}

void UChannelDataView::OnDisconnect()
{
	for (auto& Pair : ChannelDataProviders)
	{
		for (FProviderInternal& Provider : Pair.Value)
		{
			if (Provider.IsValid())
			{
				Provider->SetRemoved();
			}
		}
	}

	// Force to send the channel update data with the removed states to channeld
	SendAllChannelUpdates();
}

int32 UChannelDataView::SendAllChannelUpdates()
{
	if (Connection == nullptr)
		return 0;

	// Use the Arena for faster allocation. See https://developers.google.com/protocol-buffers/docs/reference/arenas
	google::protobuf::Arena Arena;
	int32 TotalUpdateCount = 0;
	for (auto& Pair : Connection->SubscribedChannels)
	{
		if (static_cast<channeldpb::ChannelDataAccess>(Pair.Value.SubOptions.DataAccess) == channeldpb::WRITE_ACCESS)
		{
			ChannelId ChId = Pair.Key;
			TSet<FProviderInternal>* Providers = ChannelDataProviders.Find(ChId);
			if (Providers == nullptr)
				continue;

			auto MsgTemplate = ChannelDataTemplates.FindRef(static_cast<channeldpb::ChannelType>(Pair.Value.ChannelType));
			if (MsgTemplate == nullptr)
				continue;

			auto NewState = MsgTemplate->New(&Arena);

			int UpdateCount = 0;
			int RemovedCount = 0;
			for (auto Itr = Providers->CreateIterator(); Itr; ++Itr)
			{
				auto Provider = Itr.ElementIt->Value;
				if (Provider.IsValid())
				{
					if (Provider->UpdateChannelData(NewState))
					{
						UpdateCount++;
					}
					if (Provider->IsRemoved())
					{
						Itr.RemoveCurrent();
						RemovedCount++;
					}
				}
				else
				{
					Itr.RemoveCurrent();
					RemovedCount++;
				}
			}
			if (RemovedCount > 0)
				UE_LOG(LogChanneld, Log, TEXT("Removed %d channel data provider(s) from channel %d"), RemovedCount, ChId);

			if (UpdateCount > 0 || RemovedCount > 0)
			{
				// Merge removed states
				google::protobuf::Message* RemovedData;
				if (RemovedProvidersData.RemoveAndCopyValue(ChId, RemovedData))
				{
					NewState->MergeFrom(*RemovedData);
					delete RemovedData;
				}
				
				channeldpb::ChannelDataUpdateMessage UpdateMsg;
				UpdateMsg.mutable_data()->PackFrom(*NewState);
				Connection->Send(ChId, channeldpb::CHANNEL_DATA_UPDATE, UpdateMsg);

				UE_LOG(LogChanneld, Verbose, TEXT("Sent %s update: %s"), UTF8_TO_TCHAR(NewState->GetTypeName().c_str()), UTF8_TO_TCHAR(NewState->DebugString().c_str()));
			}

			TotalUpdateCount += UpdateCount;
		}
	}

	UMetrics* Metrics = GEngine->GetEngineSubsystem<UMetrics>();
	Metrics->AddConnTypeLabel(*Metrics->ReplicatedProviders).Increment(TotalUpdateCount);

	return TotalUpdateCount;
}

UChanneldGameInstanceSubsystem* UChannelDataView::GetChanneldSubsystem()
{
	UWorld* World = GetWorld();
	// The client may still be in pending net game
	if (World == nullptr)
	{
		// The subsystem owns the view.
		return Cast<UChanneldGameInstanceSubsystem>(GetOuter());
	}
	if (World)
	{
		UGameInstance* GameInstance = World->GetGameInstance();// UGameplayStatics::GetGameInstance(this);// 
		if (GameInstance)
		{
			auto Result = GameInstance->GetSubsystem<UChanneldGameInstanceSubsystem>();
			//UE_LOG(LogChanneld, Log, TEXT("Found ChanneldGameInstanceSubsystem: %d"), Result == nullptr ? 0 : 1);
			return Result;
		}
	}
	return nullptr;
}

void UChannelDataView::HandleUnsub(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto UnsubMsg = static_cast<const channeldpb::UnsubscribedFromChannelResultMessage*>(Msg);
	if (UnsubMsg->connid() == Connection->GetConnId())
	{
		TSet<FProviderInternal> Providers;
		if (ChannelDataProviders.RemoveAndCopyValue(ChId, Providers))
		{
			UE_LOG(LogChanneld, Log, TEXT("Received Unsub message. Removed all data providers(%d) from channel %d"), Providers.Num(), ChId);
			OnUnsubFromChannel(ChId, Providers);
		}
	}
}

void UChannelDataView::HandleChannelDataUpdate(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{
	auto UpdateMsg = static_cast<const channeldpb::ChannelDataUpdateMessage*>(Msg);

	google::protobuf::Message* UpdateData;
	if (ReceivedUpdateDataInChannels.Contains(ChId))
	{
		UpdateData = ReceivedUpdateDataInChannels[ChId];
	}
	else
	{
		FString TypeUrl(UTF8_TO_TCHAR(UpdateMsg->data().type_url().c_str()));
		auto MsgTemplate = ChannelDataTemplatesByTypeUrl.FindRef(TypeUrl);
		if (MsgTemplate == nullptr)
		{
			UE_LOG(LogChanneld, Error, TEXT("Unable to find channel data parser by typeUrl: %s"), *TypeUrl);
			return;
		}
		UpdateData = MsgTemplate->New();
	}

	// Call ParsePartial instead of Parse to keep the existing value from being reset.
	if (!UpdateData->ParsePartialFromString(UpdateMsg->data().value()))
	{
		
		UE_LOG(LogChanneld, Error, TEXT("Failed to parse %s channel data, typeUrl: %s"), *GetChanneldSubsystem()->GetChannelTypeNameByChId(ChId), UTF8_TO_TCHAR(UpdateMsg->data().type_url().c_str()));
		return;
	}

	UE_LOG(LogChanneld, Verbose, TEXT("Received %s channel update: %s"), *GetChanneldSubsystem()->GetChannelTypeNameByChId(ChId), UTF8_TO_TCHAR(UpdateMsg->DebugString().c_str()));

	TSet<FProviderInternal>* Providers = ChannelDataProviders.Find(ChId);
	if (Providers == nullptr || Providers->Num() == 0)
	{
		UE_LOG(LogChanneld, Warning, TEXT("No provider registered for channel %d, typeUrl: %s"), ChId, UTF8_TO_TCHAR(UpdateMsg->data().type_url().c_str()));
		return;
	}

	// The set can be changed during the iteration, when a new provider is created from the UnrealObjectRef during any replicator's OnStateChanged(),
	// or the provider's owner actor got destroyed by removed=true. So we use a const array to iterate.
	TArray<FProviderInternal> ProvidersArr = Providers->Array();
	for (FProviderInternal& Provider : ProvidersArr)
	{
		if (Provider.IsValid() && !Provider->IsRemoved())
		{
			Provider->OnChannelDataUpdated(UpdateData);
		}
	}

	// All new states are consumed, now we reset the cached object for next parse.
	UpdateData->Clear();
}

