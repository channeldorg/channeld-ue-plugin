#include "ConeAOI.h"

#include "ChanneldUtils.h"

void FConeAOI::SetSpatialQuery(channeldpb::SpatialInterestQuery* Query, const FVector& PawnLocation, const FRotator& PawnRotation)
{
	Query->mutable_coneaoi()->mutable_center()->MergeFrom(ChanneldUtils::ToSpatialInfo(PawnLocation));
	Query->mutable_coneaoi()->mutable_direction()->MergeFrom(ChanneldUtils::ToSpatialInfo(PawnRotation.Vector()));
	Query->mutable_coneaoi()->set_radius(Radius);
	Query->mutable_coneaoi()->set_angle(Angle);
	
	UE_LOG(LogChanneld, Verbose, TEXT("Updating the ConeAOI with %s"), UTF8_TO_TCHAR(Query->mutable_coneaoi()->ShortDebugString().c_str()));
}
