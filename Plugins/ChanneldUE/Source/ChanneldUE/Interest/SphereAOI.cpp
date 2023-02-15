#include "SphereAOI.h"

#include "ChanneldUtils.h"

void FSphereAOI::SetSpatialQuery(channeldpb::SpatialInterestQuery* Query, const FVector& PawnLocation, const FRotator& PawnRotation)
{
	Query->mutable_sphereaoi()->mutable_center()->MergeFrom(ChanneldUtils::ToSpatialInfo(PawnLocation));
	Query->mutable_sphereaoi()->set_radius(Radius);
}
