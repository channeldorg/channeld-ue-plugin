#include "ClientInterestManager.h"

#include "ChanneldConnection.h"
#include "ChanneldGameInstanceSubsystem.h"
#include "ChanneldNetDriver.h"
#include "ChanneldSettings.h"
#include "SphereAOI.h"
#include "StaticLocationsAOI.h"

class UChanneldConnection;

UClientInterestManager::UClientInterestManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UClientInterestManager::ServerSetup(UChanneldNetConnection* ClientNetConn)
{
	for (auto& Preset : GetMutableDefault<UChanneldSettings>()->ClientInterestPresets)
	{
		if (Preset.AreaType == EClientInterestAreaType::Sphere)
		{
			auto AOI = NewObject<USphereAOI>(this);
			AOI->Name = Preset.PresetName;
			AOI->Radius = Preset.Radius;
			AddAOI(AOI, Preset.bActivateByDefault);
		}
		else if (Preset.AreaType == EClientInterestAreaType::StaticLocations)
		{
			auto AOI = NewObject<FStaticLocationsAOI>(this);
			AOI->Name = Preset.PresetName;
			AOI->Spots = Preset.Spots;
			// AOI->InterestedActors = InterestedActors;
			AddAOI(AOI, Preset.bActivateByDefault);
		}
	}

	ClientNetConn->PlayerEnterSpatialChannelEvent.AddUObject(this, &UClientInterestManager::OnPlayerEnterSpatialChannel);

	UE_LOG(LogChanneld, Log, TEXT("[Server] ClientInterestManager has been setup for client conn %d"), ClientNetConn->GetConnId());
}

void UClientInterestManager::CleanUp(UChanneldNetConnection* ClientNetConn)
{
	InterestedActors.Empty();
	FollowingPC = nullptr;
	AvailableAOIs.Empty();
	ActiveAOIs.Empty();
	ClientNetConn->PlayerEnterSpatialChannelEvent.RemoveAll(this);
	UE_LOG(LogChanneld, Log, TEXT("[Server] ClientInterestManager has been cleanup for client conn %d"), ClientNetConn->GetConnId());
}

void UClientInterestManager::AddAOI(UAreaOfInterestBase* AOI, bool bActivate)
{
	int Index = AvailableAOIs.Add(AOI);
	if (bActivate)
	{
		ActivateAOI(Index);
	}
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

void UClientInterestManager::FollowPlayer(APlayerController* PC, int IndexOfAOI)
{
	FollowingPC = PC;
	LastUpdateLocation = PC->GetFocalLocation();
	
	if (IndexOfAOI < 0)
	{
		for (auto& AOI : AvailableAOIs)
		{
			AOI->FollowPlayer(PC);
		}
	}
	else if (IndexOfAOI < AvailableAOIs.Num())
	{
		AvailableAOIs[IndexOfAOI]->FollowPlayer(PC);
	}
}

void UClientInterestManager::UnfollowPlayer(int IndexOfAOI)
{
	if (IndexOfAOI < 0)
	{
		for (auto& AOI : AvailableAOIs)
		{
			AOI->UnfollowPlayer();
		}
	}
	else if (IndexOfAOI < AvailableAOIs.Num())
	{
		AvailableAOIs[IndexOfAOI]->UnfollowPlayer();
	}
	
	FollowingPC = nullptr;
}

/*
bool UClientInterestManager::IsTickable() const
{
	if (IsTemplate())
	{
		return false;
	}

	if (FollowingPC.IsValid())
	{
		return true;
	}
	
	for (auto& AOI : ActiveAOIs)
	{
		if (AOI->IsTickable())
		{
			return true;
		}
	}
	return false;
}

void UClientInterestManager::Tick(float DeltaTime)
{
	if (FollowingPC.IsValid())
	{
		float DistToCheck = GetMutableDefault<UChanneldSettings>()->MinDistanceToUpdateInterestForPlayer;
		if (DistToCheck > 0)
		{
			FVector CurrentLocation = FollowingPC->GetFocalLocation();
			if (!CurrentLocation.Equals(LastUpdateLocation, DistToCheck))
			{
				OnPlayerMoved(FollowingPC.Get());
				LastUpdateLocation = CurrentLocation;
			}
		}
		else
		{
			OnPlayerMoved(FollowingPC.Get());
		}
	}
	
	for (auto& AOI : ActiveAOIs)
	{
		AOI->Tick(DeltaTime);
	}
}
*/

void UClientInterestManager::AddActorInterest(AActor* Actor)
{
	InterestedActors.Add(Actor);
}

void UClientInterestManager::RemoveActorInterest(AActor* Actor)
{
	InterestedActors.Remove(Actor);
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
		AOI->SetSpatialQuery(InterestMsg.mutable_query(), PawnLocation, PawnRotation);
	}
	
	GEngine->GetEngineSubsystem<UChanneldConnection>()->Send(SpatialChId, channeldpb::UPDATE_SPATIAL_INTEREST, InterestMsg);
}

void UClientInterestManager::OnPlayerMoved(APlayerController* PC)
{
	if (auto NetConn = Cast<UChanneldNetConnection>(PC->NetConnection))
	{
		FOwnedChannelInfo ChannelInfo;
		if (GetWorld()->GetGameInstance()->GetSubsystem<UChanneldGameInstanceSubsystem>()->GetOwningChannelInfo(PC, ChannelInfo)
			&& ChannelInfo.ChannelType == EChanneldChannelType::ECT_Spatial)
		{
			OnPlayerEnterSpatialChannel(NetConn, ChannelInfo.ChannelId);
		}
		else
		{
			UE_LOG(LogChanneld, Warning, TEXT("Player %s is not in a spatial channel."), *PC->GetName());
		}
	}
	else
	{
		UE_LOG(LogChanneld, Warning, TEXT("Player %s does not have a NetConn to update the spatial interest"), *PC->GetName());
	}
}
