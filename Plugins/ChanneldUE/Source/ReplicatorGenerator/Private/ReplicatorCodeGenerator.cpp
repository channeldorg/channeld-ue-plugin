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
	const TFunction<FString(const FString& PackageName)>* GetGoPackage,
	FGeneratedCodeBundle& ReplicatorCodeBundle
)
{
	// Clean global variables, make sure it's empty for this generation
	IllegalClassNameIndex = 0;
	TargetActorSameNameCounter.Empty(TargetActors.Num());

	// Clear global struct decorators, make sure it's empty for this generation
	FPropertyDecoratorFactory::Get().ClearGlobalStruct();

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
	ProtoFormatArgs.Add(TEXT("Declare_ProtoPackageName"), GenManager_GlobalStructProtoPackage);
	ProtoFormatArgs.Add(TEXT("Code_Import"), TEXT("import \"unreal_common.proto\";"));
	FString GoPackage = GetGoPackage == nullptr ? TEXT("") : (*GetGoPackage)(GenManager_GlobalStructProtoPackage);
	ProtoFormatArgs.Add(
		TEXT("Option"),
		GoPackage.IsEmpty() ? TEXT("") : FString::Printf(TEXT("option go_package = \"%s\";"), *GoPackage)
	);
	ProtoFormatArgs.Add(TEXT("Definition_ProtoStateMsg"), ReplicatorCodeBundle.GlobalStructProtoDefinitions);
	ReplicatorCodeBundle.GlobalStructProtoDefinitions = FString::Format(CodeGen_ProtoTemplate, ProtoFormatArgs);

	FString Message, IncludeCode, RegisterCode;
	TArray<TSharedPtr<FReplicatedActorDecorator>> ActorDecorators;
	for (const UClass* TargetActor : TargetActors)
	{
		FReplicatorCode GeneratedResult;
		if (!GenerateActorCode(TargetActor, GetGoPackage, GeneratedResult, Message))
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
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Code_IncludeActorHeaders"), IncludeCode);
	FormatArgs.Add(TEXT("Code_ReplicatorRegister"), RegisterCode);

	ReplicatorCodeBundle.ChannelDataProcessorClassName = FString::Printf(TEXT("FChannelDataProcessor_%s"), *FString::FromInt(FMath::Rand()));
	FString ChannelDataProcessorVarName = FString::Printf(TEXT("Var_%s"), *ReplicatorCodeBundle.ChannelDataProcessorClassName);
	GenerateChannelDataProcessor(ReplicatorCodeBundle, TargetActors);
	FormatArgs.Add(
		TEXT("Code_ChannelDataProcessorRegister"),
		FString::Printf(
			TEXT("%s = new %s();\nChanneldReplication::RegisterChannelDataMerger(TEXT(\"%s\"), %s);\n"),
			*ChannelDataProcessorVarName, *ReplicatorCodeBundle.ChannelDataProcessorClassName,
			*ReplicatorCodeBundle.ChannelDataProcessorProtoMsgFullName, *ChannelDataProcessorVarName
		)
	);
	FormatArgs.Add(TEXT("Code_ChannelDataProcessorUnregister"), FString::Printf(TEXT("delete %s"), *ChannelDataProcessorVarName));
	FormatArgs.Add(TEXT("Declaration_Variables"), FString::Printf(TEXT("%s* %s;\n"), *ReplicatorCodeBundle.ChannelDataProcessorClassName, *ChannelDataProcessorVarName));

	ReplicatorCodeBundle.RegisterReplicatorFileCode = FString::Format(*CodeGen_RegisterReplicatorTemplate, FormatArgs);


	return true;
}

bool FReplicatorCodeGenerator::GenerateChannelDataProcessor(FGeneratedCodeBundle& GeneratedCodeBundle, TArray<const UClass*> TargetActors)
{
	TSet<const UClass*> TargetClassWithParent;
	TArray<const UClass*> Stack = TargetActors;
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
	TArray<TSharedPtr<FReplicatedActorDecorator>> ActorChildren;
	// ChanneldDataProcessor codes
	for (const UClass* TargetActorClass : TargetClassWithParentArr)
	{
		FString ResultMessage;
		TSharedPtr<FReplicatedActorDecorator> ActorDecorator;
		// It's not necessary to initialize property decorators and rpc decorators, because ChanneldDataProcessor doesn't need them
		if (!CreateDecorateActor(ActorDecorator, ResultMessage, TargetActorClass, false, false))
		{
			UE_LOG(LogChanneldRepGenerator, Error, TEXT("%s"), *ResultMessage);
			continue;
		}
		ActorDecorators.Add(ActorDecorator);
		if (TargetActorClass->IsChildOf(AActor::StaticClass()))
		{
			ActorChildren.Add(ActorDecorator);
		}
	}
	FString ChanneldDataProcessor_IncludeCode,
	        ChanneldDataProcessor_ConstPathFNameVarDecl,
	        ChanneldDataProcessor_MergeCode,
	        ChanneldDataProcessor_GetStateCode,
	        ChanneldDataProcessor_SetStateCode;

	FString ChannelDataState = TEXT("ChannelDataState");
	int32 ConstPathFNameVarDeclIndex = 0;
	for (TSharedPtr<FReplicatedActorDecorator> ActorDecorator : ActorDecorators)
	{
		if (!ActorDecorator->IsBlueprintType())
		{
			ChanneldDataProcessor_IncludeCode.Append(FString::Printf(TEXT("#include \"%s\"\n"), *ActorDecorator->GetIncludeActorHeaderPath()));
		}
		ActorDecorator->SetConstClassPathFNameVarName(FString::Printf(TEXT("PathFName_%d"), ++ConstPathFNameVarDeclIndex));
		ChanneldDataProcessor_ConstPathFNameVarDecl.Append(ActorDecorator->GetCode_ConstPathFNameVarDecl() + TEXT("\n"));
		ChanneldDataProcessor_MergeCode.Append(ActorDecorator->GetCode_ChanneldDataProcessor_Merge(ActorChildren));
		ChanneldDataProcessor_GetStateCode.Append(
			FString::Printf(
				TEXT("%s%s"),
				ChanneldDataProcessor_GetStateCode.IsEmpty() ? TEXT("") : TEXT("else "),
				*ActorDecorator->GetCode_ChanneldDataProcessor_GetStateFromChannelData(ChannelDataState)
			)
		);
		ChanneldDataProcessor_SetStateCode.Append(
			FString::Printf(
				TEXT("%s%s"),
				ChanneldDataProcessor_SetStateCode.IsEmpty() ? TEXT("") : TEXT("else "),
				*ActorDecorator->GetCode_ChanneldDataProcessor_SetStateToChannelData(ChannelDataState)
			)
		);
	}
	FStringFormatNamedArguments CDPFormatArgs;
	// CDPFormatArgs.Add(TEXT("File_CDP_ProtoHeader"), GenManager_ChannelDataProcessorClassName + CodeGen_ProtoPbHeadExtension);
	CDPFormatArgs.Add(TEXT("Code_IncludeAdditionHeaders"), ChanneldDataProcessor_IncludeCode);
	CDPFormatArgs.Add(TEXT("Declaration_ChanneldGeneratedNamespace"), GenManager_ChannelDataProcessorNamespace);
	CDPFormatArgs.Add(TEXT("Declaration_CDP_ClassName"), GenManager_ChannelDataProcessorClassName);
	CDPFormatArgs.Add(TEXT("Definition_CDP_ProtoNamespace"), TEXT("generatedchanneldata"));
	CDPFormatArgs.Add(TEXT("Code_ConstClassPathFNameVariable"), ChanneldDataProcessor_ConstPathFNameVarDecl);
	CDPFormatArgs.Add(TEXT("Definition_ChanneldUEBuildInProtoNamespace"), GenManager_ChanneldUEBuildInProtoNamespace);
	CDPFormatArgs.Add(TEXT("Definition_CDP_ProtoNamespace"), TEXT("TODO"));
	CDPFormatArgs.Add(TEXT("Definition_CDP_ProtoMsgName"), TEXT("TODO"));
	CDPFormatArgs.Add(TEXT("Code_Merge"), ChanneldDataProcessor_MergeCode);
	CDPFormatArgs.Add(TEXT("Declaration_CDP_ProtoVar"), ChannelDataState);
	CDPFormatArgs.Add(TEXT("Code_GetStateFromChannelData"), ChanneldDataProcessor_GetStateCode);
	CDPFormatArgs.Add(TEXT("Code_SetStateToChannelData"), ChanneldDataProcessor_SetStateCode);
	GeneratedCodeBundle.ChannelDataProcessorHeadCode = FString::Format(*CodeGen_ChannelDataProcessorCPPTemp, CDPFormatArgs);
	return true;
}

bool FReplicatorCodeGenerator::GenerateActorCode(
	const UClass* TargetActorClass,
	const TFunction<FString(const FString& PackageName)>* GetGoPackage,
	FReplicatorCode& GeneratedResult,
	FString& ResultMessage
)
{
	TSharedPtr<FReplicatedActorDecorator> ActorDecorator;
	if (!CreateDecorateActor(ActorDecorator, ResultMessage, TargetActorClass))
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
	CppCodeBuilder.Append(FString::Printf(TEXT("#include \"%s\"\n\n"), *GeneratedResult.HeadFileName));

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
	FString GoPackage = GetGoPackage == nullptr ? TEXT("") : (*GetGoPackage)(ActorDecorator->GetProtoPackageName());
	ProtoFormatArgs.Add(
		TEXT("Option"),
		GoPackage.IsEmpty() ? TEXT("") : FString::Printf(TEXT("option go_package = \"%s\";"), *GoPackage)
	);
	ProtoFormatArgs.Add(
		TEXT("Definition_ProtoStateMsg"),
		ActorDecorator->GetDefinition_ProtoStateMessage()
	);
	GeneratedResult.ProtoDefinitions = FString::Format(CodeGen_ProtoTemplate, ProtoFormatArgs);

	GeneratedResult.ProtoFileName = ActorDecorator->GetActorName() + CodeGen_ProtoFileExtension;
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

bool FReplicatorCodeGenerator::CreateDecorateActor(TSharedPtr<FReplicatedActorDecorator>& OutActorDecorator, FString& OutResultMessage, const UClass* TargetActor, bool bInitPropertiesAndRPCs, bool bIncrementIfSameName)
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
		}
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
