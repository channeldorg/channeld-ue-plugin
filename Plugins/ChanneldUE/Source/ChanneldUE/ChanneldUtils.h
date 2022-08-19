#pragma once

#include "CoreMinimal.h"

namespace ChanneldUtils
{
    FVector GetVector(const channeldpb::FVector& InVec)
    {
        return FVector(InVec.x(), InVec.y(), InVec.z());
    }

	FRotator GetRotator(const channeldpb::FVector& InVec)
	{
		return FRotator(InVec.x(), InVec.y(), InVec.z());
	}
}