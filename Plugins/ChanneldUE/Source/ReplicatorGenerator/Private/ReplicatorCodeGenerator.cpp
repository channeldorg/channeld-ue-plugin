#include "ReplicatorCodeGenerator.h"

#include "Manifest.h"
#include "PropertyDecoratorFactory.h"
#include "ReplicatorGeneratorDefinition.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "ReplicatorTemplate/BlueprintReplicatorTemplate.h"
#include "ReplicatorTemplate/CppReplicatorTemplate.h"

bool FReplicatorCodeGenerator::RefreshModuleInfoByClassName()
{
	FString BuildConfiguration = ANSI_TO_TCHAR(COMPILER_CONFIGURATION_NAME);
	const FString ManifestFilePath = FPaths::ProjectIntermediateDir() / TEXT("Build") / TEXT(CHANNELD_EXPAND_AND_QUOTE(UBT_COMPILED_PLATFORM)) / TEXT(CHANNELD_EXPAND_AND_QUOTE(UE_TARGET_NAME)) / BuildConfiguration / TEXT(CHANNELD_EXPAND_AND_QUOTE(UE_TARGET_NAME)) + TEXT(".uhtmanifest");

	bool bManifestSuccessfullyLoaded;
	FManifest Manifest = FManifest::LoadFromFile(ManifestFilePath, bManifestSuccessfullyLoaded);
	if (!bManifestSuccessfullyLoaded)
	{
		return false;
	}
	for (FManifestModule& ManifestModule : Manifest.Modules)
	{
		ProcessHeaderFiles(ManifestModule.PublicUObjectClassesHeaders, ManifestModule);
		ProcessHeaderFiles(ManifestModule.PublicUObjectHeaders, ManifestModule);
		ProcessHeaderFiles(ManifestModule.PrivateUObjectHeaders, ManifestModule);
	}

	return true;
}

FString FReplicatorCodeGenerator::GetClassHeadFilePath(const FString& ClassName)
{
	FCPPClassInfo* Result = CPPClassInfoMap.Find(ClassName);
	if (Result != nullptr)
	{
		return Result->HeadFilePath;
	}
	return TEXT("");
}

bool FReplicatorCodeGenerator::Generate(TArray<UClass*> TargetActors, FReplicatorCodeBundle& ReplicatorCodeBundle)
{
	FString Message, IncludeCode, RegisterCode;
	for (UClass* TargetActor : TargetActors)
	{
		FReplicatorCode ReplicatorCode;
		GenerateActorCode(TargetActor, ReplicatorCode, Message);
		ReplicatorCodeBundle.ReplicatorCodes.Add(ReplicatorCode);
		IncludeCode += ReplicatorCode.IncludeActorCode + TEXT("\n");
		IncludeCode += FString::Printf(TEXT("#include \"%s\"\n"), *ReplicatorCode.HeadFileName);
		RegisterCode += ReplicatorCode.RegisterReplicatorCode + TEXT("\n");
	}
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Code_IncludeActorHeaders"), IncludeCode);
	FormatArgs.Add(TEXT("Code_ReplicatorRegister"), RegisterCode);
	ReplicatorCodeBundle.RegisterReplicatorFileCode = FString::Format(*CodeGen_RegisterReplicatorTemplate, FormatArgs);

	auto GlobalStructDecorators = FPropertyDecoratorFactory::Get().GetGlobalStructDecorators();
	ReplicatorCodeBundle.GlobalStructCodes.Append(TEXT("#pragma once\n"));
	ReplicatorCodeBundle.GlobalStructCodes.Append(TEXT("#include \"ChanneldUtils.h\"\n"));
	ReplicatorCodeBundle.GlobalStructCodes.Append(FString::Printf(TEXT("#include \"%s\"\n"), *GenManager_GlobalStructProtoHeaderFile));
	for (auto StructDecorator : GlobalStructDecorators)
	{
		ReplicatorCodeBundle.GlobalStructCodes.Append(StructDecorator->GetDeclaration_PropPtrGroupStruct());
		ReplicatorCodeBundle.GlobalStructProtoDefinitions.Append(StructDecorator->GetDefinition_ProtoStateMessage());
	}
	FStringFormatNamedArguments ProtoFormatArgs;
	ProtoFormatArgs.Add(TEXT("Declare_ProtoPackageName"), GenManager_GlobalStructProtoPackage);
	ProtoFormatArgs.Add(TEXT("Code_Import"), TEXT("import \"unreal_common.proto\";"));
	ProtoFormatArgs.Add(TEXT("Definition_ProtoStateMsg"), ReplicatorCodeBundle.GlobalStructProtoDefinitions);
	ReplicatorCodeBundle.GlobalStructProtoDefinitions = FString::Format(CodeGen_ProtoTemplate, ProtoFormatArgs);

	// Clear global struct decorators
	FPropertyDecoratorFactory::Get().ClearGlobalStruct();
	return true;
}


bool FReplicatorCodeGenerator::GenerateActorCode(UClass* TargetActor, FReplicatorCode& ReplicatorCode, FString& ResultMessage)
{
	int32 IllegalClassNameIndex = 0;
	const TSharedPtr<FReplicatedActorDecorator> Target = MakeShareable(
		new FReplicatedActorDecorator(
			TargetActor,
			[&IllegalClassNameIndex]()
			{
				return FString::Printf(TEXT("_IllegalNameClass_%d_"), ++IllegalClassNameIndex);
			}
		)
	);

	if (!Target->IsBlueprintType() && !ModuleInfoByClassName.Contains(Target->GetActorCPPClassName()))
	{
		ResultMessage = FString::Printf(TEXT("Can not find the module %s belongs to"), *Target->GetActorCPPClassName());
		return false;
	}

	ReplicatorCode.Target = Target;
	if (!Target->IsBlueprintType())
	{
		Target->Init(ModuleInfoByClassName.FindChecked(Target->GetActorCPPClassName()));
	}
	else
	{
		Target->Init();
	}

	bool bIsBlueprint = Target->IsBlueprintType();

	ReplicatorCode.IncludeActorCode = bIsBlueprint ? TEXT("") : FString::Printf(TEXT("#include \"%s\"\n"), *Target->GetActorHeaderIncludePath());

	FString TargetInstanceRef = TEXT("Actor");
	FString TargetBaseClassName;
	FString OnStateChangedAdditionalCondition;
	FString TickAdditionalCondition;
	FString IsClientCode;
	if (TargetActor->IsChildOf(USceneComponent::StaticClass()))
	{
		if (bIsBlueprint)
		{
			TargetBaseClassName = TEXT("USceneComponent");
		}
		TargetInstanceRef = TEXT("SenceComp");
		OnStateChangedAdditionalCondition = FString::Printf(TEXT(" || !%s->GetOwner()"), *TargetInstanceRef);
		TickAdditionalCondition = FString::Printf(TEXT(" || !%s->GetOwner()"), *TargetInstanceRef);
		IsClientCode = FString::Printf(TEXT("%s->GetOwner()->HasAuthority()"), *TargetInstanceRef);
	}
	else if (TargetActor->IsChildOf(UActorComponent::StaticClass()))
	{
		if (bIsBlueprint)
		{
			TargetBaseClassName = TEXT("UActorComponent");
		}
		TargetInstanceRef = TEXT("ActorComp");
		OnStateChangedAdditionalCondition = FString::Printf(TEXT(" || !%s->GetOwner()"), *TargetInstanceRef);
		TickAdditionalCondition = FString::Printf(TEXT(" || !%s->GetOwner()"), *TargetInstanceRef);
		IsClientCode = FString::Printf(TEXT("%s->GetOwner()->HasAuthority()"), *TargetInstanceRef);
	}
	else
	{
		if (bIsBlueprint)
		{
			TargetBaseClassName = TEXT("AActor");
		}
		IsClientCode = FString::Printf(TEXT("%s->HasAuthority()"), *TargetInstanceRef);
	}
	Target->SetInstanceRefName(TargetInstanceRef);

	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_ReplicatorClassName"), Target->GetReplicatorClassName());
	FormatArgs.Add(TEXT("Declare_TargetClassName"), Target->GetActorCPPClassName());
	FormatArgs.Add(TEXT("Ref_TargetInstanceRef"), TargetInstanceRef);
	FormatArgs.Add(TEXT("Declare_ProtoNamespace"), Target->GetProtoNamespace());
	FormatArgs.Add(TEXT("Declare_ProtoStateMsgName"), Target->GetProtoStateMessageType());
	FormatArgs.Add(TEXT("Code_OnStateChangedAdditionalCondition"), OnStateChangedAdditionalCondition);
	FormatArgs.Add(TEXT("Code_TickAdditionalCondition"), TickAdditionalCondition);
	FormatArgs.Add(TEXT("Code_IsClient"), IsClientCode);

	if (bIsBlueprint)
	{
		FormatArgs.Add(TEXT("Declare_TargetBaseClassName"), TargetBaseClassName);
	}

	FString OverrideGetNetGUIDImplCode = Target->GetCode_OverrideGetNetGUID();

	// ---------- Head code ----------
	FormatArgs.Add(TEXT("Code_IncludeActorHeader"), ReplicatorCode.IncludeActorCode);
	FormatArgs.Add(TEXT("File_ProtoPbHead"), Target->GetActorName() + CodeGen_ProtoPbHeadExtension);
	FormatArgs.Add(TEXT("Code_AdditionalInclude"), Target->GetAdditionalIncludeFiles());
	FormatArgs.Add(TEXT("Declare_IndirectlyAccessiblePropertyPtrs"), Target->GetCode_IndirectlyAccessiblePropertyPtrDeclarations());
	FormatArgs.Add(
		TEXT("Code_OverrideGetNetGUID"),
		OverrideGetNetGUIDImplCode.IsEmpty() ? TEXT("") : TEXT("virtual uint32 GetNetGUID() override;")
	);

	// RPC
	FormatArgs.Add(TEXT("Declare_OverrideSerializeAndDeserializeFunctionParams"), Target->GetRPCNum() > 0 ? CodeGen_SerializeAndDeserializeFunctionParams : TEXT(""));
	FormatArgs.Add(TEXT("Declare_RPCParamStructNamespace"), Target->GetDeclaration_RPCParamStructNamespace());
	FormatArgs.Add(TEXT("Declare_RPCParamStructs"), Target->GetDeclaration_RPCParamStructs());

	ReplicatorCode.HeadCode = FString::Format(bIsBlueprint ? CodeGen_BP_HeadCodeTemplate : CodeGen_CPP_HeadCodeTemplate, FormatArgs);
	ReplicatorCode.HeadFileName = Target->GetReplicatorClassName(false) + CodeGen_HeadFileExtension;
	// ---------- Head code ----------

	// ---------- Cpp code ----------
	FString CppCodeBuilder;
	CppCodeBuilder.Append(FString::Printf(TEXT("#include \"%s\"\n\n"), *ReplicatorCode.HeadFileName));

	FormatArgs.Add(
		TEXT("Code_AssignPropertyPointers"),
		Target->GetCode_AssignPropertyPointers()
	);
	CppCodeBuilder.Append(FString::Format(
		bIsBlueprint ? CodeGen_BP_ConstructorImplTemplate : CodeGen_CPP_ConstructorImplTemplate,
		FormatArgs
	));
	CppCodeBuilder.Append(FString::Format(CodeGen_CPP_DestructorImplTemplate, FormatArgs));
	CppCodeBuilder.Append(FString::Format(CodeGen_CPP_GetDeltaStateImplTemplate, FormatArgs));
	CppCodeBuilder.Append(FString::Format(CodeGen_CPP_ClearStateImplTemplate, FormatArgs));
	if (!OverrideGetNetGUIDImplCode.IsEmpty())
	{
		CppCodeBuilder.Append(OverrideGetNetGUIDImplCode);
	}

	FormatArgs.Add(
		TEXT("Code_AllPropertiesSetDeltaState"),
		Target->GetCode_AllPropertiesSetDeltaState(TEXT("FullState"), TEXT("DeltaState"))
	);
	CppCodeBuilder.Append(FString::Format(CodeGen_CPP_TickImplTemplate, FormatArgs));

	FormatArgs.Add(
		TEXT("Code_AllPropertyOnStateChanged"),
		Target->GetCode_AllPropertiesOnStateChange(TEXT("NewState"))
	);
	CppCodeBuilder.Append(FString::Format(CodeGen_CPP_OnStateChangedImplTemplate, FormatArgs));

	if (Target->GetRPCNum() > 0)
	{
		FormatArgs.Add(
			TEXT("Code_SerializeFunctionParams"),
			Target->GetCode_SerializeFunctionParams()
		);
		FormatArgs.Add(
			TEXT("Code_DeserializeFunctionParams"),
			Target->GetCode_DeserializeFunctionParams()
		);
		CppCodeBuilder.Append(FString::Format(CodeGen_CPP_RPCTemplate, FormatArgs));
	}

	ReplicatorCode.CppCode = CppCodeBuilder;
	ReplicatorCode.CppFileName = Target->GetReplicatorClassName(false) + CodeGen_CppFileExtension;
	// ---------- Cpp code ----------

	// ---------- Protobuf ----------
	FStringFormatNamedArguments ProtoFormatArgs;
	ProtoFormatArgs.Add(TEXT("Declare_ProtoPackageName"), Target->GetProtoPackageName());
	ProtoFormatArgs.Add(TEXT("Code_Import"),
	                    FString::Printf(TEXT("import \"%s\";\nimport \"%s\";"), *GenManager_GlobalStructProtoFile, *GenManager_UnrealCommonProtoFile)
	);
	ProtoFormatArgs.Add(
		TEXT("Definition_ProtoStateMsg"),
		Target->GetDefinition_ProtoStateMessage() + Target->GetDefinition_RPCParamsMessage()
	);
	ReplicatorCode.ProtoDefinitions = FString::Format(CodeGen_ProtoTemplate, ProtoFormatArgs);
	ReplicatorCode.ProtoFileName = Target->GetActorName() + CodeGen_ProtoFileExtension;
	// ---------- Protobuf ----------

	// ---------- Register ----------
	if (!bIsBlueprint)
	{
		ReplicatorCode.RegisterReplicatorCode = FString::Printf(
			TEXT("REGISTER_REPLICATOR(%s, %s);"),
			*Target->GetReplicatorClassName(),
			*Target->GetActorCPPClassName()
		);
	}
	else
	{
		ReplicatorCode.RegisterReplicatorCode = FString::Printf(
			TEXT("REGISTER_REPLICATOR_BP(%s, \"%s\");"),
			*Target->GetReplicatorClassName(),
			*TargetActor->GetPathName()
		);
	}
	// ---------- Register ----------

	return true;
}

void FReplicatorCodeGenerator::ProcessHeaderFiles(const TArray<FString>& Files, const FManifestModule& ManifestModule)
{
	FPlatformFileManager& FileManager = FPlatformFileManager::Get();
	for (const FString& HeaderFilePath : Files)
	{
		if (!FileManager.GetPlatformFile().FileExists(*HeaderFilePath))
		{
			continue;
		}
		FString Code;
		FFileHelper::LoadFileToString(Code, *HeaderFilePath);
		FRegexPattern MatherPatter(FString(TEXT(R"EOF(UCLASS\(.*\)\s*class\s+(?:\w+_API\s+)?([\w_]+)\s+\:)EOF")));
		FRegexMatcher Matcher(MatherPatter, Code);
		while (Matcher.FindNext())
		{
			FString CaptureString = Matcher.GetCaptureGroup(1);

			FModuleInfo ModuleInfo;
			ModuleInfo.Name = ManifestModule.Name;
			ModuleInfo.BaseDirectory = ManifestModule.BaseDirectory;
			ModuleInfo.BaseDirectory.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
			ModuleInfo.IncludeBase = ManifestModule.IncludeBase;
			ModuleInfo.IncludeBase.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
			FString RelativeToModule = HeaderFilePath;
			RelativeToModule.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
			RelativeToModule = RelativeToModule.Replace(*ModuleInfo.IncludeBase, TEXT(""), ESearchCase::CaseSensitive);
			ModuleInfo.RelativeToModule = RelativeToModule;
			ModuleInfoByClassName.Add(CaptureString, ModuleInfo);
			FCPPClassInfo CPPClassInfo;
			CPPClassInfo.ModuleInfo = ModuleInfo;
			CPPClassInfo.HeadFilePath = HeaderFilePath;
			CPPClassInfoMap.Add(CaptureString, CPPClassInfo);
		}
	}
}
