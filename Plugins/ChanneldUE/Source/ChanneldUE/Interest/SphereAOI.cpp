#include "SphereAOI.h"

#include "ChanneldUtils.h"

void USphereAOI::SetSpatialQuery(channeldpb::SpatialInterestQuery* Query, const FVector& PawnLocation, const FRotator& PawnRotation)
{
	Query->mutable_sphereaoi()->mutable_center()->MergeFrom(ChanneldUtils::ToSpatialInfo(PawnLocation));
	Query->mutable_sphereaoi()->set_radius(Radius);
	
	UE_LOG(LogChanneld, Verbose, TEXT("Updating the SphereAOI with center=%s, radius=%f"), *PawnLocation.ToCompactString(), Radius);

	// DrawDebugSphere(GetWorld(), PawnLocation, Radius, 16, FColor::Red, false, 0.1f, 0, 1.0f);
}
