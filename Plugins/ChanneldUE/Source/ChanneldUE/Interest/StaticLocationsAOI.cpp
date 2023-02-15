#include "StaticLocationsAOI.h"

#include "ChanneldUtils.h"

void FStaticLocationsAOI::SetSpatialQuery(channeldpb::SpatialInterestQuery* Query, const FVector& PawnLocation, const FRotator& PawnRotation)
{
	for (auto& Spot : Spots)
	{
		Query->mutable_spotsaoi()->add_spots()->MergeFrom(ChanneldUtils::ToSpatialInfo(Spot));
	}
	
	UE_LOG(LogChanneld, Verbose, TEXT("Updating the SpotsAOI with %d spots"), Spots.Num());
}

