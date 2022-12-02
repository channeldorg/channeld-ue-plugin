#pragma once

#define CHANNELD_QUOTE(str) #str
#define CHANNELD_EXPAND_AND_QUOTE(str) CHANNELD_QUOTE(str)

static const FString CodeGen_HeadFileExtension = TEXT(".h");
static const FString CodeGen_CppFileExtension = TEXT(".cpp");
static const FString CodeGen_ProtoFileExtension = TEXT(".proto");
static const FString CodeGen_ProtoPbHeadExtension = TEXT(".pb.h");

DECLARE_LOG_CATEGORY_EXTERN(LogChanneldReplicatorGenerator, Warning, All);

static const FString GenManager_GeneratedCodeDir = TEXT("ChanneldGenerated");

static const FName GenManager_ProtobufModuleName = TEXT("ProtobufEditor");
