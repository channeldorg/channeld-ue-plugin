#include "PlayerFollowingAOI.h"

void UPlayerFollowingAOI::FollowActor(AActor* Target)
{
	if (auto PC = Cast<APlayerController>(Target))
	{
		FollowingPC = PC;
		LastUpdateLocation = PC->GetFocalLocation();
	}
}

void UPlayerFollowingAOI::UnfollowActor(AActor* Target)
{
	if (Target == FollowingPC)
	{
		FollowingPC = nullptr;
	}
}

bool UPlayerFollowingAOI::TickQuery(channeldpb::SpatialInterestQuery* Query, float DeltaTime)
{
	if (FollowingPC.IsValid())
	{
		FVector CurrentLocation = FollowingPC->GetFocalLocation();
		if (MinDistanceToTriggerUpdate > 0)
		{
			if (!CurrentLocation.Equals(LastUpdateLocation, MinDistanceToTriggerUpdate))
			{
				SetSpatialQuery(Query, CurrentLocation, FollowingPC->GetControlRotation());
				LastUpdateLocation = CurrentLocation;
				return true;
			}
		}
		else
		{
			SetSpatialQuery(Query, CurrentLocation, FollowingPC->GetControlRotation());
			return true;
		}
	}
	
	return false;
}
