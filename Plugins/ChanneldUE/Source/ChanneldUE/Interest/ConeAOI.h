#pragma once

#include "AreaOfInterestBase.h"

class CHANNELDUE_API FConeAOI : public UAreaOfInterestBase
{
public:
	float Radius;
	float Angle;
	
protected:
	virtual void SetSpatialQuery(channeldpb::SpatialInterestQuery* Query, const FVector& PawnLocation, const FRotator& PawnRotation) override;
};