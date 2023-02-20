#include "StaticLocationsAOI.h"

#include "ChanneldUtils.h"

void UStaticLocationsAOI::SetSpatialQuery(channeldpb::SpatialInterestQuery* Query, const FVector& PawnLocation, const FRotator& PawnRotation)
{
	for (auto& Key : SpotsAndDists)
	{
		ChanneldUtils::SetSpatialInfoPB(Query->mutable_spotsaoi()->add_spots(), Key.Key);
		Query->mutable_spotsaoi()->add_dists(Key.Value);
	}
	
	UE_LOG(LogChanneld, Verbose, TEXT("Updating the SpotsAOI with %d spots"), SpotsAndDists.Num());
}
