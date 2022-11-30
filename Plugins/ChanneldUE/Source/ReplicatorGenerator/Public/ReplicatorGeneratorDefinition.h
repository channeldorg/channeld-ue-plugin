#pragma once

#define CHANNELD_QUOTE(str) #str
#define CHANNELD_EXPAND_AND_QUOTE(str) CHANNELD_QUOTE(str)

DECLARE_LOG_CATEGORY_EXTERN(LogChanneldReplicatorGenerator, Warning, All);

static const FString GenManager_GeneratedCodeDir = TEXT("ChanneldGenerated");

static const FName GenManager_ProtobufModuleName = TEXT("ProtobufEditor");
