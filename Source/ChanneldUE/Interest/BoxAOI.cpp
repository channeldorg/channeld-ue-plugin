#include "BoxAOI.h"

#include "ChanneldUtils.h"

void UBoxAOI::SetSpatialQuery(channeldpb::SpatialInterestQuery* Query, const FVector& PawnLocation, const FRotator& PawnRotation)
{
	ChanneldUtils::SetSpatialInfoPB(Query->mutable_boxaoi()->mutable_center(), PawnLocation);
	ChanneldUtils::SetSpatialInfoPB(Query->mutable_boxaoi()->mutable_extent(), Extent);
	
	UE_LOG(LogChanneld, Verbose, TEXT("Updating the BoxAOI with center=%s, Extent=%s"), *PawnLocation.ToCompactString(), *Extent.ToCompactString());
}
