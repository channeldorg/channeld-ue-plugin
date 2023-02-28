#pragma once

REPLICATORGENERATOR_API DECLARE_LOG_CATEGORY_EXTERN(LogChanneldRepGenerator, Log, All);

#define CHANNELD_QUOTE(str) #str
#define CHANNELD_EXPAND_AND_QUOTE(str) CHANNELD_QUOTE(str)

static const FString CodeGen_HeadFileExtension = TEXT(".h");
static const FString CodeGen_CppFileExtension = TEXT(".cpp");
static const FString CodeGen_ProtoFileExtension = TEXT(".proto");
static const FString CodeGen_ProtoPbHeadExtension = TEXT(".pb.h");
static const FString CodeGen_ProtoPbCPPExtension = TEXT(".pb.cpp");

static const FString GenManager_GeneratedCodeDir = TEXT("ChanneldGenerated");
static const FString GenManager_ChanneldUEBuildInProtoNamespace = TEXT("unrealpb");
static const FString GenManager_RepRegisterFile = TEXT("ChanneldReplicatorRegister") + CodeGen_HeadFileExtension;
static const FString GenManager_GlobalStructHeaderFile = TEXT("ChanneldGlobalStruct") + CodeGen_HeadFileExtension;
static const FString GenManager_GlobalStructProtoFile = TEXT("ChanneldGlobalStruct") + CodeGen_ProtoFileExtension;
static const FString GenManager_UnrealCommonProtoFile = TEXT("unreal_common") + CodeGen_ProtoFileExtension;
static const FString GenManager_GlobalStructProtoHeaderFile = TEXT("ChanneldGlobalStruct") + CodeGen_ProtoPbHeadExtension;

static const FString GenManager_IntermediateDir = FPaths::ProjectIntermediateDir() / TEXT("ChanneldReplicatorGenerator");
static const FString GenManager_PrevCodeGeneratedInfoPath = GenManager_IntermediateDir / TEXT("PrevCodeGeneratedInfoPath.json");
static const FString GenManager_RepClassInfoPath = GenManager_IntermediateDir / TEXT("RepAssetInfoPath.json");
