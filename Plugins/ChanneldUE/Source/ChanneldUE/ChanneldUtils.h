#pragma once

#include "CoreMinimal.h"
#include "ChannelDataProvider.h"

namespace ChanneldUtils
{
    FString GetProviderName(const IChannelDataProvider* Provider)
    {
        return dynamic_cast<const UObjectBase*>(Provider)->GetClass()->GetName();
    }
}