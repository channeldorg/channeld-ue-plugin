#include "ClientInterestManager.h"

#include "ChanneldConnection.h"
#include "ChanneldGameInstanceSubsystem.h"
#include "ChanneldNetDriver.h"
#include "ChanneldSettings.h"
#include "ConeAOI.h"
#include "SphereAOI.h"
#include "StaticLocationsAOI.h"

class UChanneldConnection;

UClientInterestManager::UClientInterestManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// QueryForTick = new channeldpb::SpatialInterestQuery;
}

void UClientInterestManager::ServerSetup(UChanneldNetConnection* InClientNetConn)
{
	ClientNetConn = InClientNetConn;
	
	for (auto& Preset : GetMutableDefault<UChanneldSettings>()->ClientInterestPresets)
	{
		UAreaOfInterestBase* AOI = nullptr;
		
		if (Preset.AreaType == EClientInterestAreaType::Sphere)
		{
			auto Sphere = NewObject<USphereAOI>(this);
			AOI = Sphere;
			Sphere->Radius = Preset.Radius;
		}
		else if (Preset.AreaType == EClientInterestAreaType::Cone)
		{
			auto Cone = NewObject<UConeAOI>(this);
			AOI = Cone;
			Cone->Radius = Preset.Radius;
			Cone->Angle = Preset.Angle;
		}
		else if (Preset.AreaType == EClientInterestAreaType::StaticLocations)
		{
			auto StaticLoc = NewObject<UStaticLocationsAOI>(this);
			AOI = StaticLoc;
			StaticLoc->SpotsAndDists = Preset.SpotsAndDists;
		}

		if (AOI)
		{
			AOI->Name = Preset.PresetName;
			AOI->MinDistanceToTriggerUpdate = Preset.MinDistanceToTriggerUpdate;
			AddAOI(AOI, Preset.bActivateByDefault);
		}
	}

	InClientNetConn->PlayerEnterSpatialChannelEvent.AddUObject(this, &UClientInterestManager::OnPlayerEnterSpatialChannel);

	UE_LOG(LogChanneld, Log, TEXT("[Server] ClientInterestManager has been setup for client conn %d"), InClientNetConn->GetConnId());
}

void UClientInterestManager::CleanUp()
{
	AvailableAOIs.Empty();
	ActiveAOIs.Empty();
	// QueryForTick->Clear();
	// delete QueryForTick;
	if (ClientNetConn.IsValid())
	{
		ClientNetConn->PlayerEnterSpatialChannelEvent.RemoveAll(this);
		UE_LOG(LogChanneld, Log, TEXT("[Server] ClientInterestManager has been cleanup for client conn %d"), ClientNetConn->GetConnId());
	}
}

void UClientInterestManager::AddAOI(UAreaOfInterestBase* AOI, bool bActivate)
{
	int Index = AvailableAOIs.Add(AOI);
	if (bActivate)
	{
		ActivateAOI(Index);
	}
}

UAreaOfInterestBase* UClientInterestManager::GetAOIByIndex(int Index)
{
	if (Index < 0 || Index >= AvailableAOIs.Num())
	{
		return nullptr;
	}
	return AvailableAOIs[Index];
}

UAreaOfInterestBase* UClientInterestManager::GetAOIByName(const FName& Name)
{
	for (auto& AOI : AvailableAOIs)
	{
		if (AOI->Name == Name)
		{
			return AOI;
		}
	}
	return nullptr;
}

void UClientInterestManager::ActivateAOI(int Index)
{
	if (Index < 0 || Index >= AvailableAOIs.Num())
	{
		return;
	}
	
	ActiveAOIs.Add(AvailableAOIs[Index]);
	AvailableAOIs[Index]->OnActivate();
}

void UClientInterestManager::DeactivateAOI(int Index)
{
	if (Index < 0 || Index >= AvailableAOIs.Num())
	{
		return;
	}
	
	for (int i = 0; i < ActiveAOIs.Num(); i++)
	{
		if (ActiveAOIs[i] == AvailableAOIs[Index])
		{
			ActiveAOIs[i]->OnDeactivate();
			ActiveAOIs.RemoveAt(i);
			break;
		}
	}
}

void UClientInterestManager::FollowActor(AActor* Target, int IndexOfAOI)
{
	if (IndexOfAOI < 0)
	{
		for (auto& AOI : AvailableAOIs)
		{
			AOI->FollowActor(Target);
		}
	}
	else if (IndexOfAOI < AvailableAOIs.Num())
	{
		AvailableAOIs[IndexOfAOI]->FollowActor(Target);
	}
}

void UClientInterestManager::UnfollowActor(AActor* Target, int IndexOfAOI)
{
	if (IndexOfAOI < 0)
	{
		for (auto& AOI : AvailableAOIs)
		{
			AOI->UnfollowActor(Target);
		}
	}
	else if (IndexOfAOI < AvailableAOIs.Num())
	{
		AvailableAOIs[IndexOfAOI]->UnfollowActor(Target);
	}
}

bool UClientInterestManager::IsTickable() const
{
	if (IsTemplate())
	{
		return false;
	}
	
	return ClientNetConn.IsValid();
}

void UClientInterestManager::Tick(float DeltaTime)
{
	bool bNewQuery = false;
	for (auto& AOI : ActiveAOIs)
	{
		bNewQuery |= AOI->TickQuery(&QueryForTick, DeltaTime);
	}

	if (bNewQuery)
	{
		channeldpb::UpdateSpatialInterestMessage InterestMsg;
		InterestMsg.set_connid(ClientNetConn->GetConnId());
		InterestMsg.mutable_query()->MergeFrom(QueryForTick);
		GEngine->GetEngineSubsystem<UChanneldConnection>()->Send(ClientNetConn->GetSendToChannelId(), channeldpb::UPDATE_SPATIAL_INTEREST, InterestMsg);
		QueryForTick.Clear();
	}
}

void UClientInterestManager::OnPlayerEnterSpatialChannel(UChanneldNetConnection* NetConn, Channeld::ChannelId SpatialChId)
{
	channeldpb::UpdateSpatialInterestMessage InterestMsg;
	InterestMsg.set_connid(NetConn->GetConnId());
	FVector PawnLocation;
	FRotator PawnRotation;
	if (NetConn->PlayerController->GetPawn())
	{
		PawnLocation = NetConn->PlayerController->GetPawn()->GetActorLocation();
		PawnRotation = NetConn->PlayerController->GetPawn()->GetActorRotation();
	}
	else
	{
		PawnLocation = NetConn->PlayerController->GetSpawnLocation();
		PawnRotation = FRotator::ZeroRotator;
	}
	
	for (auto& AOI : ActiveAOIs)
	{
		// AOI->OnPlayerEnterSpatialChannel(NetConn, SpatialChId);
		AOI->SetSpatialQuery(InterestMsg.mutable_query(), PawnLocation, PawnRotation);
	}
	
	GEngine->GetEngineSubsystem<UChanneldConnection>()->Send(SpatialChId, channeldpb::UPDATE_SPATIAL_INTEREST, InterestMsg);
}
