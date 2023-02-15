#include "ClientInterestManager.h"

#include "ChanneldConnection.h"
#include "ChanneldSettings.h"
#include "SphereAOI.h"

class UChanneldConnection;

bool UClientInterestManager::IsTickable() const
{
	if (IsTemplate())
	{
		return false;
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
	for (auto& AOI : ActiveAOIs)
	{
		AOI->Tick(DeltaTime);
	}
}

void UClientInterestManager::ServerSetup(UChanneldNetConnection* ClientNetConn)
{
	for (auto& Preset : GetMutableDefault<UChanneldSettings>()->ClientInterestPresets)
	{
		if (Preset.AreaType == EClientInterestAreaType::Sphere)
		{
			auto AOI = MakeShared<FSphereAOI>();
			AOI->Name = Preset.PresetName;
			AOI->Radius = Preset.Radius;
			AddAOI(AOI, Preset.bActivateByDefault);
		}
	}

	ClientNetConn->PlayerEnterSpatialChannelEvent.AddUObject(this, &UClientInterestManager::OnPlayerEnterSpatialChannel);

	UE_LOG(LogChanneld, Log, TEXT("[Server] ClientInterestManager has been setup for client conn %d"), ClientNetConn->GetConnId());
}

void UClientInterestManager::CleanUp(UChanneldNetConnection* ClientNetConn)
{
	AvailableAOIs.Empty();
	ActiveAOIs.Empty();
	ClientNetConn->PlayerEnterSpatialChannelEvent.RemoveAll(this);
	UE_LOG(LogChanneld, Log, TEXT("[Server] ClientInterestManager has been cleanup for client conn %d"), ClientNetConn->GetConnId());
}

void UClientInterestManager::AddAOI(TSharedPtr<FAreaOfInterestBase> AOI, bool bActivate)
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
}

void UClientInterestManager::AddActorInterest(AActor* Actor)
{
}

void UClientInterestManager::RemoveActorInterest(AActor* Actor)
{
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
