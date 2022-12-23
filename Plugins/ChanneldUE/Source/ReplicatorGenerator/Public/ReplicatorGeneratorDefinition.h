#pragma once

DECLARE_LOG_CATEGORY_EXTERN(LogChanneldRepGenerator, Warning, All);

#define CHANNELD_QUOTE(str) #str
#define CHANNELD_EXPAND_AND_QUOTE(str) CHANNELD_QUOTE(str)

static const FString CodeGen_HeadFileExtension = TEXT(".h");
static const FString CodeGen_CppFileExtension = TEXT(".cpp");
static const FString CodeGen_ProtoFileExtension = TEXT(".proto");
static const FString CodeGen_ProtoPbHeadExtension = TEXT(".pb.h");
static const FString CodeGen_ProtoPbCPPExtension = TEXT(".pb.cpp");

static const FString GenManager_GeneratedCodeDir = TEXT("ChanneldGenerated");
static const FName GenManager_ProtobufModuleName = TEXT("ProtobufEditor");
static const FString GenManager_RepRegisterFile = TEXT("ChanneldReplicatorRegister") + CodeGen_HeadFileExtension;
static const FString GenManager_GlobalStructHeaderFile = TEXT("ChanneldGlobalStruct") + CodeGen_HeadFileExtension;
static const FString GenManager_GlobalStructProtoFile = TEXT("ChanneldGlobalStruct") + CodeGen_ProtoFileExtension;
static const FString GenManager_GlobalStructProtoHeaderFile = TEXT("ChanneldGlobalStruct") + CodeGen_ProtoPbHeadExtension;
static const FString GenManager_GlobalStructProtoNamespace = TEXT("channeldglobalstructpb");
static const FString GenManager_GlobalStructProtoPackage = GenManager_GlobalStructProtoNamespace;

static const FString GenManager_PrevCodeGeneratedInfoPath = FPaths::ProjectIntermediateDir() / TEXT("ChanneldPrevCodeGeneratedInfoPath.json");