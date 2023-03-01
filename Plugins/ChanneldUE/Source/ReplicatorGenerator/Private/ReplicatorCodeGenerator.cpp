#include "ReplicatorCodeGenerator.h"

#include "Manifest.h"
#include "PropertyDecoratorFactory.h"
#include "ReplicatorGeneratorDefinition.h"
#include "ReplicatorGeneratorUtils.h"
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
		UE_LOG(LogChanneldRepGenerator, Error, TEXT("Failed to load manifest file: %s"), *ManifestFilePath);
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

bool FReplicatorCodeGenerator::Generate(
	TArray<const UClass*> TargetActors,
	const FString& DefaultModuleDir,
	const FString& ProtoPackageName,
	const FString& GoPackageImportPath,
	FGeneratedCodeBundle& ReplicatorCodeBundle
)
{
	// Clean global variables, make sure it's empty for this generation
	IllegalClassNameIndex = 0;
	TargetActorSameNameCounter.Empty(TargetActors.Num());

	// Clear global struct decorators, make sure it's empty for this generation
	FPropertyDecoratorFactory::Get().ClearGlobalStruct();

	FString Message, IncludeCode, RegisterCode;
	TArray<TSharedPtr<FReplicatedActorDecorator>> ActorDecorators;
	for (const UClass* TargetActor : TargetActors)
	{
		FReplicatorCode GeneratedResult;
		if (!GenerateActorCode(TargetActor, ProtoPackageName, GoPackageImportPath, GeneratedResult, Message))
		{
			UE_LOG(LogChanneldRepGenerator, Error, TEXT("%s"), *Message);
			continue;
		}
		ReplicatorCodeBundle.ReplicatorCodes.Add(GeneratedResult);
		ActorDecorators.Add(GeneratedResult.ActorDecorator);
		IncludeCode.Append(GeneratedResult.IncludeActorCode + TEXT("\n"));
		IncludeCode.Append(FString::Printf(TEXT("#include \"%s\"\n"), *GeneratedResult.HeadFileName));
		RegisterCode.Append(GeneratedResult.RegisterReplicatorCode + TEXT("\n"));
	}

	// Channel data processor
	const FString DefaultModuleName = FPaths::GetBaseFilename(DefaultModuleDir);
	IncludeCode.Append(FString::Printf(TEXT("#include \"ChannelData_%s.h\"\n"), *DefaultModuleName));
	const FString CDPNamespace = DefaultModuleName + TEXT("Processor");
	const FString CDPClassName = TEXT("F") + CDPNamespace;
	const FString ChanneldDataProtoMsgName = GenManager_DefaultChannelDataMsgName;
	if (!GenerateChannelDataCode(
			TargetActors,
			ChanneldDataProtoMsgName, CDPNamespace, CDPClassName,
			TEXT("ChannelData_") + DefaultModuleName + CodeGen_ProtoPbHeadExtension,
			ProtoPackageName, GoPackageImportPath,
			ReplicatorCodeBundle, Message
		)
	)
	{
		UE_LOG(LogChanneldRepGenerator, Error, TEXT("%s"), *Message);
	}

	// Register codes
	FStringFormatNamedArguments RegisterFormatArgs;
	// register replicators
	RegisterFormatArgs.Add(TEXT("Code_IncludeActorHeaders"), IncludeCode);
	RegisterFormatArgs.Add(TEXT("Code_ReplicatorRegister"), RegisterCode);

	// register channel data processor
	const FString CDPVarNameInRegister = FString::Printf(TEXT("Var_%s"), *CDPClassName);
	RegisterFormatArgs.Add(
		TEXT("Code_ChannelDataProcessorRegister"),
		FString::Printf(
			TEXT("%s = new %s::%s();\nChanneldReplication::RegisterChannelDataMerger(TEXT(\"%s.%s\"), %s);\n"),
			*CDPVarNameInRegister,
			*CDPNamespace, *CDPClassName,
			*ProtoPackageName, *ChanneldDataProtoMsgName,
			*CDPVarNameInRegister
		)
	);
	RegisterFormatArgs.Add(TEXT("Code_ChannelDataProcessorUnregister"), FString::Printf(TEXT("delete %s;"), *CDPVarNameInRegister));
	RegisterFormatArgs.Add(TEXT("Declaration_Variables"), FString::Printf(TEXT("%s::%s* %s;\n"), *CDPNamespace, *CDPClassName, *CDPVarNameInRegister));
	ReplicatorCodeBundle.ReplicatorRegistrationHeadCode = FString::Format(*CodeGen_ReplicatorRegistrationTemp, RegisterFormatArgs);

	// Type definitions
	ReplicatorCodeBundle.TypeDefinitionsHeadCode = CodeGen_ChanneldGeneratedTypesHeadTemp;
	ReplicatorCodeBundle.TypeDefinitionsCppCode = FString::Printf(TEXT("#include \"%s\"\nDEFINE_LOG_CATEGORY(LogChanneldGen);"), *GenManager_TypeDefinitionHeadFile);

	// Global struct codes
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
	ProtoFormatArgs.Add(TEXT("Declare_ProtoPackageName"), ProtoPackageName);
	ProtoFormatArgs.Add(TEXT("Code_Import"), TEXT("import \"unreal_common.proto\";"));
	ProtoFormatArgs.Add(
		TEXT("Option"),
		GoPackageImportPath.IsEmpty() ? TEXT("") : FString::Printf(TEXT("option go_package = \"%s\";"), *GoPackageImportPath)
	);
	ProtoFormatArgs.Add(TEXT("Definition_ProtoStateMsg"), ReplicatorCodeBundle.GlobalStructProtoDefinitions);
	ReplicatorCodeBundle.GlobalStructProtoDefinitions = FString::Format(CodeGen_ProtoTemplate, ProtoFormatArgs);

	return true;
}

bool FReplicatorCodeGenerator::GenerateActorCode(
	const UClass* TargetActorClass,
	const FString& ProtoPackageName,
	const FString& GoPackageImportPath,
	FReplicatorCode& GeneratedResult,
	FString& ResultMessage
)
{
	TSharedPtr<FReplicatedActorDecorator> ActorDecorator;
	if (!CreateDecorateActor(ActorDecorator, ResultMessage, TargetActorClass, ProtoPackageName, GoPackageImportPath))
	{
		return false;
	}

	GeneratedResult.ActorDecorator = ActorDecorator;

	bool bIsBlueprint = ActorDecorator->IsBlueprintType();

	GeneratedResult.IncludeActorCode = bIsBlueprint ? TEXT("") : FString::Printf(TEXT("#include \"%s\"\n"), *ActorDecorator->GetIncludeActorHeaderPath());

	FString TargetInstanceRef = TEXT("Actor");
	FString TargetBaseClassName;
	FString OnStateChangedAdditionalCondition;
	FString TickAdditionalCondition;
	FString IsClientCode;
	if (TargetActorClass->IsChildOf(USceneComponent::StaticClass()))
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
	else if (TargetActorClass->IsChildOf(UActorComponent::StaticClass()))
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
	ActorDecorator->SetInstanceRefName(TargetInstanceRef);

	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_ReplicatorClassName"), ActorDecorator->GetReplicatorClassName());
	FormatArgs.Add(TEXT("Declare_TargetClassName"), ActorDecorator->GetActorCPPClassName());
	FormatArgs.Add(TEXT("Ref_TargetInstanceRef"), TargetInstanceRef);
	FormatArgs.Add(TEXT("Declare_ProtoNamespace"), ActorDecorator->GetProtoNamespace());
	FormatArgs.Add(TEXT("Declare_ProtoStateMsgName"), ActorDecorator->GetProtoStateMessageType());
	FormatArgs.Add(TEXT("Code_OnStateChangedAdditionalCondition"), OnStateChangedAdditionalCondition);
	FormatArgs.Add(TEXT("Code_TickAdditionalCondition"), TickAdditionalCondition);
	FormatArgs.Add(TEXT("Code_IsClient"), IsClientCode);

	if (bIsBlueprint)
	{
		FormatArgs.Add(TEXT("Declare_TargetBaseClassName"), TargetBaseClassName);
	}

	FString OverrideGetNetGUIDImplCode = ActorDecorator->GetCode_OverrideGetNetGUID();

	// ---------- Head code ----------
	FormatArgs.Add(TEXT("Code_IncludeActorHeader"), GeneratedResult.IncludeActorCode);
	FormatArgs.Add(TEXT("File_ProtoPbHead"), ActorDecorator->GetActorName() + CodeGen_ProtoPbHeadExtension);
	FormatArgs.Add(TEXT("Code_AdditionalInclude"), ActorDecorator->GetAdditionalIncludeFiles());
	FormatArgs.Add(TEXT("Declare_IndirectlyAccessiblePropertyPtrs"), ActorDecorator->GetCode_IndirectlyAccessiblePropertyPtrDeclarations());
	FormatArgs.Add(
		TEXT("Code_OverrideGetNetGUID"),
		OverrideGetNetGUIDImplCode.IsEmpty() ? TEXT("") : TEXT("virtual uint32 GetNetGUID() override;")
	);

	// RPC
	FormatArgs.Add(TEXT("Declare_OverrideSerializeAndDeserializeFunctionParams"), ActorDecorator->GetRPCNum() > 0 ? CodeGen_SerializeAndDeserializeFunctionParams : TEXT(""));
	FormatArgs.Add(TEXT("Declare_RPCParamStructNamespace"), ActorDecorator->GetDeclaration_RPCParamStructNamespace());
	FormatArgs.Add(TEXT("Declare_RPCParamStructs"), ActorDecorator->GetDeclaration_RPCParamStructs());

	GeneratedResult.HeadCode = FString::Format(bIsBlueprint ? CodeGen_BP_HeadCodeTemplate : CodeGen_CPP_HeadCodeTemplate, FormatArgs);
	GeneratedResult.HeadFileName = ActorDecorator->GetReplicatorClassName(false) + CodeGen_HeadFileExtension;
	// ---------- Head code ----------

	// ---------- Cpp code ----------
	FString CppCodeBuilder;
	CppCodeBuilder.Append(FString::Printf(TEXT("#include \"%s\"\n"), *GeneratedResult.HeadFileName));
	CppCodeBuilder.Append(FString::Printf(TEXT("#include \"%s\"\n\n"), *GenManager_TypeDefinitionHeadFile));

	FormatArgs.Add(
		TEXT("Code_AssignPropertyPointers"),
		ActorDecorator->GetCode_AssignPropertyPointers()
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
		ActorDecorator->GetCode_AllPropertiesSetDeltaState(TEXT("FullState"), TEXT("DeltaState"))
	);
	CppCodeBuilder.Append(FString::Format(CodeGen_CPP_TickImplTemplate, FormatArgs));

	FormatArgs.Add(
		TEXT("Code_AllPropertyOnStateChanged"),
		ActorDecorator->GetCode_AllPropertiesOnStateChange(TEXT("NewState"))
	);
	CppCodeBuilder.Append(FString::Format(CodeGen_CPP_OnStateChangedImplTemplate, FormatArgs));

	if (ActorDecorator->GetRPCNum() > 0)
	{
		FormatArgs.Add(
			TEXT("Code_SerializeFunctionParams"),
			ActorDecorator->GetCode_SerializeFunctionParams()
		);
		FormatArgs.Add(
			TEXT("Code_DeserializeFunctionParams"),
			ActorDecorator->GetCode_DeserializeFunctionParams()
		);
		CppCodeBuilder.Append(FString::Format(CodeGen_CPP_RPCTemplate, FormatArgs));
	}

	GeneratedResult.CppCode = CppCodeBuilder;
	GeneratedResult.CppFileName = ActorDecorator->GetReplicatorClassName(false) + CodeGen_CppFileExtension;
	// ---------- Cpp code ----------

	// ---------- Protobuf ----------
	FStringFormatNamedArguments ProtoFormatArgs;
	ProtoFormatArgs.Add(TEXT("Declare_ProtoPackageName"), ActorDecorator->GetProtoPackageName());
	ProtoFormatArgs.Add(TEXT("Code_Import"),
	                    FString::Printf(TEXT("import \"%s\";\nimport \"%s\";"), *GenManager_GlobalStructProtoFile, *GenManager_UnrealCommonProtoFile)
	);
	ProtoFormatArgs.Add(
		TEXT("Option"),
		ActorDecorator->GetGoPackageImportPath().IsEmpty() ? TEXT("") : FString::Printf(TEXT("option go_package = \"%s\";"), *ActorDecorator->GetGoPackageImportPath())
	);
	ProtoFormatArgs.Add(
		TEXT("Definition_ProtoStateMsg"),
		ActorDecorator->GetDefinition_ProtoStateMessage()
	);
	GeneratedResult.ProtoDefinitionsFile = FString::Format(CodeGen_ProtoTemplate, ProtoFormatArgs);

	GeneratedResult.ProtoFileName = ActorDecorator->GetProtoDefinitionsFileName();
	// ---------- Protobuf ----------

	// ---------- Register ----------
	FString IsMapInChannelData = ActorDecorator->IsSingleton() ? TEXT("false") : TEXT("true");
	if (!bIsBlueprint)
	{
		GeneratedResult.RegisterReplicatorCode = FString::Printf(
			TEXT("REGISTER_REPLICATOR_BASE(%s, %s, false, %s);"),
			*ActorDecorator->GetReplicatorClassName(),
			*ActorDecorator->GetActorCPPClassName(),
			*IsMapInChannelData
		);
	}
	else
	{
		GeneratedResult.RegisterReplicatorCode = FString::Printf(
			TEXT("REGISTER_REPLICATOR_BP_BASE(%s, \"%s\", false, %s);"),
			*ActorDecorator->GetReplicatorClassName(),
			*TargetActorClass->GetPathName(),
			*IsMapInChannelData
		);
	}
	// ---------- Register ----------

	return true;
}

bool FReplicatorCodeGenerator::GenerateChannelDataCode(
	TArray<const UClass*> TargetActorClasses,
	const FString& ChannelDataProtoMsgName,
	const FString& ChannelDataProcessorNamespace,
	const FString& ChannelDataProcessorClassName,
	const FString& ChannelDataProtoHeadFileName,
	const FString& ProtoPackageName,
	const FString& GoPackageImportPath,
	FGeneratedCodeBundle& GeneratedCodeBundle,
	FString& ResultMessage
)
{
	// Get all actor class decorator  with their parent classes
	TSet<const UClass*> TargetClassWithParent;
	TArray<const UClass*> Stack = TargetActorClasses;
	const UClass* CurrentClass;
	while (Stack.Num() > 0)
	{
		CurrentClass = Stack.Pop();

		if (CurrentClass == nullptr || CurrentClass == AActor::StaticClass() || TargetClassWithParent.Contains(CurrentClass))
		{
			continue;
		}
		if (ChanneldReplicatorGeneratorUtils::HasReplicatedProperty(CurrentClass))
		{
			TargetClassWithParent.Add(CurrentClass);
		}
		if (CurrentClass != AActor::StaticClass() || CurrentClass != UActorComponent::StaticClass())
		{
			if (CurrentClass->GetSuperClass() != AActor::StaticClass())
			{
				Stack.Add(CurrentClass->GetSuperClass());
			}
			if (!CurrentClass->IsChildOf(UActorComponent::StaticClass()))
			{
				TArray<const UClass*> CompClasses = ChanneldReplicatorGeneratorUtils::GetComponentClasses(CurrentClass);
				if (CompClasses.Num() > 0)
				{
					Stack.Append(CompClasses);
				}
			}
		}
	}
	TArray<const UClass*> TargetClassWithParentArr = TargetClassWithParent.Array();
	TargetClassWithParentArr.Add(AActor::StaticClass());

	TArray<TSharedPtr<FReplicatedActorDecorator>> ActorDecorators;
	TArray<TSharedPtr<FReplicatedActorDecorator>> ChildrenOfAActor;
	for (const UClass* TargetActorClass : TargetClassWithParentArr)
	{
		FString ResultMessage;
		TSharedPtr<FReplicatedActorDecorator> ActorDecorator;
		// It's not necessary to initialize property decorators and rpc decorators, because ChannelDataProcessor doesn't need them
		if (!CreateDecorateActor(ActorDecorator, ResultMessage, TargetActorClass, ProtoPackageName, GoPackageImportPath, false, false))
		{
			UE_LOG(LogChanneldRepGenerator, Error, TEXT("%s"), *ResultMessage);
			continue;
		}
		ActorDecorators.Add(ActorDecorator);
		if (TargetActorClass->IsChildOf(AActor::StaticClass()))
		{
			ChildrenOfAActor.Add(ActorDecorator);
		}
	}

	// Generate ChannelDataProcessor CPP code
	GenerateChannelDataProcessorCode(
		ActorDecorators,
		ChildrenOfAActor,
		ChannelDataProtoMsgName,
		ChannelDataProcessorNamespace,
		ChannelDataProcessorClassName,
		ChannelDataProtoHeadFileName,
		ProtoPackageName,
		GeneratedCodeBundle.ChannelDataProcessorHeadCode
	);
	// Generate ChannelDataProcessor Proto definition file
	GenerateChannelDataProtoDefFile(
		ActorDecorators,
		ChannelDataProtoMsgName,
		ProtoPackageName, GoPackageImportPath,
		GeneratedCodeBundle.ChannelDataProtoDefsFile
	);

	GenerateGolangMergeCode(
		ActorDecorators,
		ChildrenOfAActor,
		ChannelDataProtoMsgName,
		ProtoPackageName,
		GeneratedCodeBundle.ChannelDataGolangMergeCode
	);

	return true;
}

bool FReplicatorCodeGenerator::GenerateChannelDataProcessorCode(
	const TArray<TSharedPtr<FReplicatedActorDecorator>>& TargetActors,
	const TArray<TSharedPtr<FReplicatedActorDecorator>>& ChildrenOfAActor,
	const FString& ChannelDataMessageName,
	const FString& ChannelDataProcessorNamespace,
	const FString& ChannelDataProcessorClassName,
	const FString& ChannelDataProtoHeadFileName,
	const FString& ProtoPackageName,
	FString& ChannelDataProcessorCode
)
{
	FString ChannelDataProcessor_IncludeCode = FString::Printf(TEXT("#include \"%s\"\n"), *GenManager_TypeDefinitionHeadFile);
	FString ChannelDataProcessor_ConstPathFNameVarDecl;
	FString ChannelDataProcessor_MergeCode;
	FString ChannelDataProcessor_GetStateCode;
	FString ChannelDataProcessor_SetStateCode;

	int32 ConstPathFNameVarDeclIndex = 0;
	for (const TSharedPtr<FReplicatedActorDecorator> ActorDecorator : TargetActors)
	{
		if (!ActorDecorator->IsBlueprintType())
		{
			ChannelDataProcessor_IncludeCode.Append(FString::Printf(TEXT("#include \"%s\"\n"), *ActorDecorator->GetIncludeActorHeaderPath()));
		}
		ActorDecorator->SetConstClassPathFNameVarName(FString::Printf(TEXT("PathFName_%d"), ++ConstPathFNameVarDeclIndex));
		ChannelDataProcessor_ConstPathFNameVarDecl.Append(ActorDecorator->GetCode_ConstPathFNameVarDecl() + TEXT("\n"));
		ChannelDataProcessor_MergeCode.Append(ActorDecorator->GetCode_ChannelDataProcessor_Merge(ChildrenOfAActor));

		ChannelDataProcessor_GetStateCode.Append(
			FString::Printf(
				TEXT("%s%s"),
				ChannelDataProcessor_GetStateCode.IsEmpty() ? TEXT("") : TEXT("else "),
				*ActorDecorator->GetCode_ChannelDataProcessor_GetStateFromChannelData(ChannelDataMessageName)
			)
		);
		ChannelDataProcessor_SetStateCode.Append(
			FString::Printf(
				TEXT("%s%s"),
				ChannelDataProcessor_SetStateCode.IsEmpty() ? TEXT("") : TEXT("else "),
				*ActorDecorator->GetCode_ChannelDataProcessor_SetStateToChannelData(ChannelDataMessageName)
			)
		);
	}
	FStringFormatNamedArguments CDPFormatArgs;
	CDPFormatArgs.Add(TEXT("Code_IncludeAdditionHeaders"), ChannelDataProcessor_IncludeCode);
	CDPFormatArgs.Add(TEXT("File_CDP_ProtoHeader"), ChannelDataProtoHeadFileName);
	CDPFormatArgs.Add(TEXT("Declaration_CDP_Namespace"), ChannelDataProcessorNamespace);
	CDPFormatArgs.Add(TEXT("Declaration_CDP_ClassName"), ChannelDataProcessorClassName);
	CDPFormatArgs.Add(TEXT("Definition_CDP_ProtoNamespace"), TEXT("generatedchanneldata"));
	CDPFormatArgs.Add(TEXT("Code_ConstClassPathFNameVariable"), ChannelDataProcessor_ConstPathFNameVarDecl);
	CDPFormatArgs.Add(TEXT("Definition_ChanneldUEBuildInProtoNamespace"), GenManager_ChanneldUEBuildInProtoPackageName);
	CDPFormatArgs.Add(TEXT("Definition_CDP_ProtoNamespace"), ProtoPackageName);
	CDPFormatArgs.Add(TEXT("Definition_CDP_ProtoMsgName"), ChannelDataMessageName);
	CDPFormatArgs.Add(TEXT("Code_Merge"), ChannelDataProcessor_MergeCode);
	CDPFormatArgs.Add(TEXT("Declaration_CDP_ProtoVar"), ChannelDataMessageName);
	CDPFormatArgs.Add(TEXT("Code_GetStateFromChannelData"), ChannelDataProcessor_GetStateCode);
	CDPFormatArgs.Add(TEXT("Code_SetStateToChannelData"), ChannelDataProcessor_SetStateCode);
	ChannelDataProcessorCode = FString::Format(*CodeGen_ChannelDataProcessorCPPTemp, CDPFormatArgs);
	return true;
}

bool FReplicatorCodeGenerator::GenerateChannelDataProtoDefFile(
	const TArray<TSharedPtr<FReplicatedActorDecorator>>& TargetActors,
	const FString& ChannelDataMessageName,
	const FString& ProtoPackageName,
	const FString& GoPackageImportPath,
	FString& ChannelDataProtoFile
)
{
	FString ChannelDataFields;
	FString ImportCode = FString::Printf(TEXT("import \"%s\";\n"), *GenManager_UnrealCommonProtoFile);
	int32 I = 0;
	for (const TSharedPtr<FReplicatedActorDecorator> ActorDecorator : TargetActors)
	{
		ChannelDataFields.Append(ActorDecorator->GetCode_ChannelDataProtoFieldDefinition(++I));
		if (!ActorDecorator->IsChanneldUEBuiltinType())
		{
			ImportCode.Append(FString::Printf(TEXT("import \"%s\";\n"), *ActorDecorator->GetProtoDefinitionsFileName()));
		}
	}
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Code_Import"), ImportCode);
	FormatArgs.Add(TEXT("Option"), FString::Printf(TEXT("option go_package = \"%s\";\n"), *GoPackageImportPath));
	FormatArgs.Add(TEXT("Declare_ProtoPackageName"), ProtoPackageName);
	FormatArgs.Add(TEXT("Definition_ProtoStateMsg"), FString::Printf(TEXT("message %s {\n%s}\n"), *ChannelDataMessageName, *ChannelDataFields));
	ChannelDataProtoFile = FString::Format(CodeGen_ProtoTemplate, FormatArgs);
	return true;
}

bool FReplicatorCodeGenerator::GenerateGolangMergeCode(const TArray<TSharedPtr<FReplicatedActorDecorator>>& TargetActors, const TArray<TSharedPtr<FReplicatedActorDecorator>>& ChildrenOfAActor, const FString& ChannelDataMessageName, const FString& ProtoPackageName, FString& GolangMergeCode)
{
	// TODO : Generate Golang Merge Code
	GolangMergeCode = TEXT("");
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
		// Load source code files, and capture all 'UCLASS' marked classes.
		FFileHelper::LoadFileToString(Code, *HeaderFilePath);
		FRegexPattern MatherPatter(FString(TEXT(R"EOF(UCLASS\(.*\)\s*class\s+(?:\w+_API\s+)?([\w_]+)\s+\:)EOF")));
		FRegexMatcher Matcher(MatherPatter, Code);
		while (Matcher.FindNext())
		{
			FString CapturedCppClassName = Matcher.GetCaptureGroup(1);

			FModuleInfo ModuleInfo;
			ModuleInfo.Name = ManifestModule.Name;
			// Normalize path
			ModuleInfo.BaseDirectory = ManifestModule.BaseDirectory;
			ModuleInfo.BaseDirectory.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
			ModuleInfo.IncludeBase = ManifestModule.IncludeBase;
			ModuleInfo.IncludeBase.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
			FString RelativeToModule = HeaderFilePath;
			RelativeToModule.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
			RelativeToModule = RelativeToModule.Replace(*ModuleInfo.IncludeBase, TEXT(""), ESearchCase::CaseSensitive);
			ModuleInfo.RelativeToModule = RelativeToModule;
			ModuleInfo.bIsBuildInEngine = FPaths::IsUnderDirectory(ModuleInfo.IncludeBase, FPaths::EngineDir());
			ModuleInfoByClassName.Add(CapturedCppClassName, ModuleInfo);
			FCPPClassInfo CPPClassInfo;
			CPPClassInfo.ModuleInfo = ModuleInfo;
			CPPClassInfo.HeadFilePath = HeaderFilePath;
			CPPClassInfoMap.Add(CapturedCppClassName, CPPClassInfo);
		}
	}
}

bool FReplicatorCodeGenerator::CreateDecorateActor(
	TSharedPtr<FReplicatedActorDecorator>& OutActorDecorator,
	FString& OutResultMessage,
	const UClass* TargetActor,
	const FString& ProtoPackageName,
	const FString& GoPackageImportPath,
	bool bInitPropertiesAndRPCs,
	bool bIncrementIfSameName
)
{
	FReplicatedActorDecorator* ActorDecorator = new FReplicatedActorDecorator(
		TargetActor,
		[this, bIncrementIfSameName](FString& TargetActorName, bool IsActorNameCompilable)
		{
			if (!IsActorNameCompilable)
			{
				TargetActorName = FString::Printf(TEXT("_IllegalNameClass_%d_"), ++IllegalClassNameIndex);
			}
			else if (bIncrementIfSameName)
			{
				int32* SameNameCount;
				if ((SameNameCount = TargetActorSameNameCounter.Find(TargetActorName)) != nullptr)
				{
					TargetActorName = FString::Printf(TEXT("%s_%d"), *TargetActorName, (*SameNameCount)++);
				}
				else
				{
					TargetActorSameNameCounter.Add(TargetActorName, 1);
				}
			}
		},
		ProtoPackageName,
		GoPackageImportPath
	);
	// If the target class is c++ class, we need to find the module it belongs to.
	// The module info is used to generate the include code in head file.
	if (!ActorDecorator->IsBlueprintType() && !ModuleInfoByClassName.Contains(ActorDecorator->GetActorCPPClassName()))
	{
		OutResultMessage = FString::Printf(TEXT("Can not find the module %s belongs to"), *ActorDecorator->GetActorCPPClassName());
		delete ActorDecorator;
		ActorDecorator = nullptr;
		OutActorDecorator = nullptr;
		return false;
	}
	if (!ActorDecorator->IsBlueprintType())
	{
		ActorDecorator->SetModuleInfo(ModuleInfoByClassName.FindChecked(ActorDecorator->GetActorCPPClassName()));
	}
	if (bInitPropertiesAndRPCs)
	{
		ActorDecorator->InitPropertiesAndRPCs();
	}
	OutActorDecorator = MakeShareable(ActorDecorator);
	return true;
}
