#include "ReplicatorCodeGenerator.h"

#include "Manifest.h"
#include "PropertyDecoratorFactory.h"
#include "ReplicatorGeneratorDefinition.h"
#include "ReplicatorGeneratorUtils.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "ReplicatorTemplate/BlueprintReplicatorTemplate.h"
#include "ReplicatorTemplate/CppReplicatorTemplate.h"
#include "ReplicatorTemplate/GoProtoDataTemplate.h"

bool FReplicatorCodeGenerator::RefreshModuleInfoByClassName()
{
	const FString BuildConfiguration = ANSI_TO_TCHAR(COMPILER_CONFIGURATION_NAME);
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
	const TArray<FRepGenActorInfo>& ReplicationActorInfos,
	const FString& DefaultModuleDir,
	const FString& ProtoPackageName,
	const FString& GoPackageImportPath,
	FGeneratedCodeBundle& ReplicatorCodeBundle
)
{
	// Clean global variables, make sure it's empty for this generation
	IllegalClassNameIndex = 0;
	TargetActorSameNameCounter.Empty(ReplicationActorInfos.Num());

	// Clear global struct decorators, make sure it's empty for this generation
	FPropertyDecoratorFactory::Get().ClearGlobalStruct();

	FString Message, IncludeCode, RegisterCode;
	TArray<TSharedPtr<FReplicatedActorDecorator>> ActorDecoratorsToGenReplicator;
	TArray<TSharedPtr<FReplicatedActorDecorator>> ActorDecoratorsToGenChannelData;
	for (const FRepGenActorInfo& Info : ReplicationActorInfos)
	{
		const UClass* ReplicationActorClass = Info.TargetActorClass;
		TSharedPtr<FReplicatedActorDecorator> ActorDecorator;
		if (!ChanneldReplicatorGeneratorUtils::IsChanneldUEBuiltinClass(ReplicationActorClass))
		{
			if (!CreateDecorateActor(ActorDecorator, Message, Info, ProtoPackageName, GoPackageImportPath))
			{
				UE_LOG(LogChanneldRepGenerator, Error, TEXT("%s"), *Message);
				continue;
			}
			if (ActorDecorator->IsSkipGenReplicator())
			{
				UE_LOG(LogChanneldRepGenerator, Display, TEXT("Skip generate replicator for %s"), *ActorDecorator->GetActorPathName());
				continue;
			}
			FReplicatorCode GeneratedResult;
			if (!GenerateActorCode(ActorDecorator, GeneratedResult, Message))
			{
				UE_LOG(LogChanneldRepGenerator, Error, TEXT("%s"), *Message);
				continue;
			}
			ReplicatorCodeBundle.ReplicatorCodes.Add(GeneratedResult);
			ActorDecoratorsToGenReplicator.Add(ActorDecorator);
			IncludeCode.Append(GeneratedResult.IncludeActorCode + TEXT("\n"));
			IncludeCode.Append(FString::Printf(TEXT("#include \"%s\"\n"), *GeneratedResult.HeadFileName));
			RegisterCode.Append(GeneratedResult.RegisterReplicatorCode + TEXT("\n"));
		}
		else
		{
			if (!CreateDecorateActor(ActorDecorator, Message, Info, ProtoPackageName, GoPackageImportPath, false))
			{
				UE_LOG(LogChanneldRepGenerator, Error, TEXT("%s"), *Message);
				continue;
			}
		}
		if (ActorDecorator->IsSkipGenChannelDataState())
		{
			UE_LOG(LogChanneldRepGenerator, Display, TEXT("Skip generate channel data field for %s"), *ActorDecorator->GetActorPathName());
		}
		else
		{
			ActorDecoratorsToGenChannelData.Add(ActorDecorator);
		}
	}

	// Channel data processor
	const FString DefaultModuleName = FPaths::GetBaseFilename(DefaultModuleDir);
	IncludeCode.Append(FString::Printf(TEXT("#include \"ChannelData_%s.h\"\n"), *DefaultModuleName));
	const FString CDPNamespace = DefaultModuleName + TEXT("Processor");
	const FString CDPClassName = TEXT("F") + CDPNamespace;
	const FString ChanneldDataProtoMsgName = GenManager_DefaultChannelDataMsgName;
	if (!GenerateChannelDataCode(
			ActorDecoratorsToGenChannelData,
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
			TEXT("%s = new %s::%s();\nChanneldReplication::RegisterChannelDataProcessor(TEXT(\"%s.%s\"), %s);\n"),
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
	for (auto ActorDecorator : ActorDecoratorsToGenReplicator)
	{
		ReplicatorCodeBundle.GlobalStructCodes.Append(ActorDecorator->GetDeclaration_RPCParamStructs());
		ReplicatorCodeBundle.GlobalStructProtoDefinitions.Append(ActorDecorator->GetDefinition_RPCParamProtoDefinitions());
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
	const TSharedPtr<FReplicatedActorDecorator>& ActorDecorator,
	FReplicatorCode& GeneratedResult,
	FString& ResultMessage
)
{
	GeneratedResult.ActorDecorator = ActorDecorator;
	const UClass* TargetActorClass = ActorDecorator->GetTargetClass();

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
	const TArray<TSharedPtr<FReplicatedActorDecorator>>& ReplicationActorDecorators,
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
	TArray<TSharedPtr<FReplicatedActorDecorator>> SortedReplicationActorClasses = ReplicationActorDecorators;
	TArray<TSharedPtr<FReplicatedActorDecorator>> ChildrenOfAActor;

	int32 AActorIndex = INDEX_NONE;
	for (int32 i = 0; i < SortedReplicationActorClasses.Num(); ++i)
	{
		const UClass* TargetClass = SortedReplicationActorClasses[i]->GetTargetClass();
		if (TargetClass == AActor::StaticClass())
		{
			AActorIndex = i;
		}
		else if (TargetClass->IsChildOf(AActor::StaticClass()))
		{
			ChildrenOfAActor.Add(SortedReplicationActorClasses[i]);
		}
	}
	// In runtime, all AActor children's state will be removed from channel data while AActor's state is removed.
	// If we remove AActor children's states before merge them, the AActor's state will add to channel data again.
	// So we need to make sure AActor is the last one in the list for generating merge channel data code.
	if (AActorIndex == INDEX_NONE)
	{
		UE_LOG(LogChanneldRepGenerator, Warning, TEXT("AActor is not included in channel data"));
	}
	else
	{
		Swap(SortedReplicationActorClasses[AActorIndex], SortedReplicationActorClasses[SortedReplicationActorClasses.Num() - 1]);
	}

	// Generate ChannelDataProcessor CPP code
	if (!GenerateChannelDataProcessorCode(
		SortedReplicationActorClasses,
		ChildrenOfAActor,
		ChannelDataProtoMsgName,
		ChannelDataProcessorNamespace,
		ChannelDataProcessorClassName,
		ChannelDataProtoHeadFileName,
		ProtoPackageName,
		GeneratedCodeBundle.ChannelDataProcessorHeadCode
	))
	{
		ResultMessage = TEXT("Failed to generate ChannelDataProcessor code");
		return false;
	}

	// Generate ChannelDataProcessor Proto definition file
	if (!GenerateChannelDataProtoDefFile(
		SortedReplicationActorClasses,
		ChannelDataProtoMsgName,
		ProtoPackageName, GoPackageImportPath,
		GeneratedCodeBundle.ChannelDataProtoDefsFile
	))
	{
		ResultMessage = TEXT("Failed to generate channel data proto definition file");
		return false;
	}

	if (!GenerateChannelDataMerge_GoCode(
		SortedReplicationActorClasses,
		ChildrenOfAActor,
		ChannelDataProtoMsgName,
		ProtoPackageName,
		GeneratedCodeBundle.ChannelDataMerge_GoCode
	))
	{
		ResultMessage = TEXT("Failed to generate channel data merge go code");
		return false;
	}

	if (!GenerateChannelDataRegistration_GoCode(
		GoPackageImportPath,
		ChannelDataProtoMsgName,
		ProtoPackageName,
		GeneratedCodeBundle.ChannelDataRegistration_GoCode
	))
	{
		ResultMessage = TEXT("Failed to generate channel data registration go code");
		return false;
	}

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
	FString ChannelDataProcessor_RemovedStateDecl;
	FString ChannelDataProcessor_InitRemovedStateCode;
	FString ChannelDataProcessor_MergeCode;
	FString ChannelDataProcessor_GetStateCode;
	FString ChannelDataProcessor_SetStateCode;
	FString ChannelDataProcessor_GetRelevantNetGUIDsCode;

	int32 ConstPathFNameVarDeclIndex = 0;
	for (const TSharedPtr<FReplicatedActorDecorator> ActorDecorator : TargetActors)
	{
		if (!ActorDecorator->IsBlueprintType())
		{
			ChannelDataProcessor_IncludeCode.Append(FString::Printf(TEXT("#include \"%s\"\n"), *ActorDecorator->GetIncludeActorHeaderPath()));
		}
		ActorDecorator->SetConstClassPathFNameVarName(FString::Printf(TEXT("PathFName_%d"), ++ConstPathFNameVarDeclIndex));
		ChannelDataProcessor_ConstPathFNameVarDecl.Append(ActorDecorator->GetCode_ConstPathFNameVarDecl() + TEXT("\n"));
		ChannelDataProcessor_RemovedStateDecl.Append(ActorDecorator->GetDeclaration_ChanneldDataProcessor_RemovedStata() + TEXT("\n"));
		ChannelDataProcessor_InitRemovedStateCode.Append(ActorDecorator->GetCode_ChanneldDataProcessor_InitRemovedState());
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

		const UClass* TargetClass = ActorDecorator->GetTargetClass();
		if ((TargetClass != AActor::StaticClass() &&
				TargetClass != UActorComponent::StaticClass() &&
				!TargetClass->IsChildOf(AGameStateBase::StaticClass()) &&
				!TargetClass->IsChildOf(APlayerState::StaticClass()) &&
				!TargetClass->IsChildOf(AController::StaticClass()) &&
				!ActorDecorator->IsSingleton()) ||
			TargetClass->IsChildOf(USceneComponent::StaticClass())
		)
		{
			FStringFormatNamedArguments FormatArgs;
			FormatArgs.Add(TEXT("Declaration_CDP_ProtoVar"), ChannelDataMessageName);
			FormatArgs.Add(TEXT("Definition_StateMapName"), ActorDecorator->GetDefinition_ChannelDataFieldNameCpp());
			ChannelDataProcessor_GetRelevantNetGUIDsCode.Append(FString::Format(CodeGen_GetRelevantNetIdByStateTemplate, FormatArgs));
		}
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
	CDPFormatArgs.Add(TEXT("Declaration_RemovedState"), ChannelDataProcessor_RemovedStateDecl);
	CDPFormatArgs.Add(TEXT("Code_InitRemovedState"), ChannelDataProcessor_InitRemovedStateCode);
	CDPFormatArgs.Add(TEXT("Definition_CDP_ProtoMsgName"), ChannelDataMessageName);

	CDPFormatArgs.Add(TEXT("Code_Merge"), ChannelDataProcessor_MergeCode);
	CDPFormatArgs.Add(TEXT("Declaration_CDP_ProtoVar"), ChannelDataMessageName);
	CDPFormatArgs.Add(TEXT("Code_GetStateFromChannelData"), ChannelDataProcessor_GetStateCode);
	CDPFormatArgs.Add(TEXT("Code_SetStateToChannelData"), ChannelDataProcessor_SetStateCode);
	CDPFormatArgs.Add(TEXT("Code_GetRelevantNetGUIDs"), ChannelDataProcessor_GetRelevantNetGUIDsCode);
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

bool FReplicatorCodeGenerator::GenerateChannelDataMerge_GoCode(
	const TArray<TSharedPtr<FReplicatedActorDecorator>>& TargetActors,
	const TArray<TSharedPtr<FReplicatedActorDecorator>>& ChildrenOfAActor,
	const FString& ChannelDataMessageName,
	const FString& ProtoPackageName,
	FString& GoCode
)
{
	GoCode = TEXT("");
	GoCode.Append(FString::Printf(TEXT("package %s\n"), *ProtoPackageName));
	GoCode.Append(CodeGen_Go_Data_ImportTemplate);

	// Generate code: Implement [channeld.ChannelDataCollector]
	{
		FStringFormatNamedArguments FormatArgs;
		FormatArgs.Add("Definition_ChannelDataMsgName", ChannelDataMessageName);

		GoCode.Append(FString::Format(CodeGen_Go_CollectStatesTemplate, FormatArgs));
	}
	{
		FStringFormatNamedArguments FormatArgs;
		for (const TSharedPtr<FReplicatedActorDecorator> ActorDecorator : TargetActors)
		{
			// No need to collect singleton state for handover
			if (ActorDecorator->IsSingleton())
			{
				continue;
			}
			FormatArgs.Add("Definition_StatePackagePath", ActorDecorator->GetProtoPackagePathGo(ProtoPackageName));
			FString StateClassName = ActorDecorator->GetProtoStateMessageType();
			FormatArgs.Add("Definition_StateClassName", StateClassName);
			FormatArgs.Add("Definition_StateVarName", "new" + StateClassName);
			FormatArgs.Add("Definition_StateMapName", ActorDecorator->GetDefinition_ChannelDataFieldNameGo());

			GoCode.Append(FString::Format(CodeGen_Go_CollectStateInMapTemplate, FormatArgs));
		}
	}

	GoCode.Append(TEXT("\treturn nil\n}\n"));


	// Generate code: Implement [channeld.MergeableChannelData]
	{
		FStringFormatNamedArguments FormatArgs;
		FormatArgs.Add("Definition_ChannelDataMsgName", ChannelDataMessageName);

		GoCode.Append(FString::Format(CodeGen_Go_MergeTemplate, FormatArgs));
	}
	{
		FStringFormatNamedArguments FormatArgs;

		FString MergeActorStateCode = TEXT("");

		for (const TSharedPtr<FReplicatedActorDecorator> ActorDecorator : TargetActors)
		{
			FormatArgs.Add("Definition_StatePackagePath", ActorDecorator->GetProtoPackagePathGo(ProtoPackageName));
			FString StateClassName = ActorDecorator->GetProtoStateMessageType();
			FormatArgs.Add("Definition_StateClassName", StateClassName);

			if (ActorDecorator->IsSingleton())
			{
				FormatArgs.Add("Definition_StateVarName", StateClassName);

				GoCode.Append(FString::Format(CodeGen_Go_MergeStateTemplate, FormatArgs));
			}
			else
			{
				FormatArgs.Add("Definition_NewStateVarName", "new" + StateClassName);
				FormatArgs.Add("Definition_OldStateVarName", "old" + StateClassName);
				FormatArgs.Add("Definition_StateMapName", ActorDecorator->GetDefinition_ChannelDataFieldNameGo());

				if (ActorDecorator->GetTargetClass() == AActor::StaticClass())
				{
					FString DeleteStateCode = TEXT("");
					FStringFormatNamedArguments DeletionFormatArgs;
					for (const auto& Deletion : TargetActors)
					{
						if (Deletion->IsSingleton())
						{
							continue;
						}
						if (Deletion->GetTargetClass()->IsChildOf<UActorComponent>())
						{
							continue;
						}
						DeletionFormatArgs.Add("Definition_StateMapName", Deletion->GetDefinition_ChannelDataFieldNameGo());
						DeleteStateCode.Append(FString::Format(CodeGen_Go_DeleteStateInMapTemplate, DeletionFormatArgs));
					}
					FormatArgs.Add("Code_DeleteFromStates", DeleteStateCode);
					// Don't add the code yet
					MergeActorStateCode = FString::Format(CodeGen_Go_MergeActorStateInMapTemplate, FormatArgs);
				}
				else
				{
					GoCode.Append(FString::Format(ActorDecorator->GetTargetClass()->IsChildOf<UActorComponent>() ? CodeGen_Go_MergeCompStateInMapTemplate : CodeGen_Go_MergeStateInMapTemplate, FormatArgs));
				}
			}
		}

		// Add ActorState's merge code at last
		GoCode.Append(MergeActorStateCode);
	}

	GoCode.Append(TEXT("\treturn nil\n}\n"));

	return true;
}

bool FReplicatorCodeGenerator::GenerateChannelDataRegistration_GoCode(
	const FString& GoImportPath,
	const FString& ChannelDataMessageName,
	const FString& ProtoPackageName,
	FString& GoCode
)
{
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add("Definition_GoImportPath", GoImportPath);
	FormatArgs.Add("Definition_GenPackageName", ProtoPackageName);
	FormatArgs.Add("Definition_ChannelDataMsgName", ChannelDataMessageName);
	GoCode = FString::Format(CodeGen_Go_RegistrationTemplate, FormatArgs);

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
	const FRepGenActorInfo& ReplicationActorInfo,
	const FString& ProtoPackageName,
	const FString& GoPackageImportPath,
	bool bInitPropertiesAndRPCs
)
{
	FReplicatedActorDecorator* ActorDecorator = new FReplicatedActorDecorator(
		ReplicationActorInfo.TargetActorClass
		, [this, ReplicationActorInfo](FString& TargetActorName, bool IsActorNameCompilable)
		{
			if (!IsActorNameCompilable)
			{
				TargetActorName = FString::Printf(TEXT("_IllegalNameClass_%d_"), ++IllegalClassNameIndex);
			}
			int32* SameNameCount;
			if ((SameNameCount = TargetActorSameNameCounter.Find(TargetActorName)) != nullptr)
			{
				*SameNameCount += 1;
				TargetActorName = FString::Printf(TEXT("%s_%s"), *TargetActorName, *ChanneldReplicatorGeneratorUtils::GetHashString(ReplicationActorInfo.TargetActorClass->GetPathName()));
			}
			else
			{
				TargetActorSameNameCounter.Add(TargetActorName, 1);
			}
		}
		, ProtoPackageName
		, GoPackageImportPath
		, ReplicationActorInfo.bSingleton
		, ReplicationActorInfo.bChanneldUEBuiltinType
		, ReplicationActorInfo.bSkipGenReplicator
		, ReplicationActorInfo.bSkipGenChannelDataState
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
