#include "ConeAOI.h"

#include "ChanneldUtils.h"

void UConeAOI::FollowActor(AActor* Target)
{
	Super::FollowActor(Target);
	
	if (FollowingPC.IsValid())
	{
		LastUpdateRotation = FollowingPC->GetControlRotation();
	}
}

void UConeAOI::SetSpatialQuery(channeldpb::SpatialInterestQuery* Query, const FVector& PawnLocation, const FRotator& PawnRotation)
{
	ChanneldUtils::SetSpatialInfoPB(Query->mutable_coneaoi()->mutable_center(), PawnLocation);
	ChanneldUtils::SetSpatialInfoPB(Query->mutable_coneaoi()->mutable_direction(), PawnRotation.Vector());
	Query->mutable_coneaoi()->set_radius(Radius);
	Query->mutable_coneaoi()->set_angle(Angle);
	
	UE_LOG(LogChanneld, Verbose, TEXT("Updating the ConeAOI with %s"), UTF8_TO_TCHAR(Query->mutable_coneaoi()->ShortDebugString().c_str()));
}

bool UConeAOI::TickQuery(channeldpb::SpatialInterestQuery* Query, float DeltaTime)
{
	if (FollowingPC.IsValid())
	{
		FRotator CurrentRotation = FollowingPC->GetControlRotation();
		if (!CurrentRotation.Equals(LastUpdateRotation))
		{
			SetSpatialQuery(Query, FollowingPC->GetFocalLocation(), CurrentRotation);
			LastUpdateRotation = CurrentRotation;
			return true;
		}
		
		return Super::TickQuery(Query, DeltaTime);
	}
	
	return false;
}
