#pragma once

#include "AreaOfInterestBase.h"

class FSphereAOI : public FAreaOfInterestBase
{
public:
	float Radius;
	
protected:
	virtual void SetSpatialQuery(channeldpb::SpatialInterestQuery* Query, const FVector& PawnLocation, const FRotator& PawnRotation) override;
};