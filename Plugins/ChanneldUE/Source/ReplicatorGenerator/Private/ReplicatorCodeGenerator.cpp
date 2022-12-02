#include "ReplicatorCodeGenerator.h"

#include "Manifest.h"
#include "ReplicatorGeneratorDefinition.h"
#include "Kismet/KismetSystemLibrary.h"
#include "ReplicatorTemplate/BlueprintActorTemplate.h"
#include "ReplicatorTemplate/CppActorTemplate.h"

bool FReplicatorCodeGenerator::RefreshModuleInfoByClassName()
{
#if defined(UE_BUILD_DEBUG) && UE_BUILD_DEBUG == 1
	FString BuildConfiguration = TEXT("Debug");
#elif defined(UE_BUILD_DEVELOPMENT) && UE_BUILD_DEVELOPMENT == 1
	FString BuildConfiguration = TEXT("Development");
#else
	FString BuildConfiguration = TEXT("");
#endif

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

bool FReplicatorCodeGenerator::Generate(TArray<UClass*> TargetActors, FReplicatorCodeBundle& ReplicatorCodeBundle)
{
	RefreshModuleInfoByClassName();
	FString Message;
	FString IncludeCode;
	FString RegisterCode;
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
	FormatArgs.Add(TEXT("Code_IncludeActorHeaders"), FStringFormatArg(IncludeCode));
	FormatArgs.Add(TEXT("Code_ReplicatorRegister"), FStringFormatArg(RegisterCode));
	ReplicatorCodeBundle.RegisterReplicatorFileCode = FString::Format(*CodeGen_RegisterReplicatorTemplate, FormatArgs);

	return true;
}


bool FReplicatorCodeGenerator::GenerateActorCode(UClass* TargetActor, FReplicatorCode& ReplicatorCode, FString& ResultMessage)
{
	const TSharedPtr<FReplicatedActorDecorator> Target = MakeShareable(new FReplicatedActorDecorator(TargetActor));

	if (!Target->IsBlueprintGenerated() && !ModuleInfoByClassName.Contains(Target->GetActorCPPClassName()))
	{
		ResultMessage = FString::Printf(TEXT("Can not find the module %s belongs to"), *Target->GetActorCPPClassName());
		return false;
	}

	ReplicatorCode.Target = Target;
	if (!Target->IsBlueprintGenerated())
	{
		Target->Init(ModuleInfoByClassName.FindChecked(Target->GetActorCPPClassName()));
	}
	else
	{
		Target->Init();
	}

	bool bIsBlueprint = Target->IsBlueprintGenerated();

	ReplicatorCode.IncludeActorCode = bIsBlueprint ? TEXT("") : FString::Printf(TEXT("#include \"%s\""), *Target->GetActorHeaderIncludePath());

	const FString TargetInstanceRef = TEXT("Actor");
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_ReplicatorClassName"), FStringFormatArg(Target->GetReplicatorClassName()));
	FormatArgs.Add(TEXT("Declare_TargetClassName"), FStringFormatArg(Target->GetActorCPPClassName()));
	FormatArgs.Add(TEXT("Ref_TargetInstanceRef"), FStringFormatArg(TargetInstanceRef));
	FormatArgs.Add(TEXT("Declare_ProtoNamespace"), FStringFormatArg(Target->GetProtoNamespace()));
	FormatArgs.Add(TEXT("Declare_ProtoStateMsgName"), FStringFormatArg(Target->GetProtoStateMessageType()));
	FormatArgs.Add(TEXT("Code_IncludeActorHeader"), FStringFormatArg(ReplicatorCode.IncludeActorCode));

	// ---------- Head code ----------
	FormatArgs.Add(TEXT("File_ProtoPbHead"), FStringFormatArg(Target->GetActorName() + CodeGen_ProtoPbHeadExtension));
	FormatArgs.Add(TEXT("Declare_IndirectlyAccessiblePropertyPtrs"), FStringFormatArg(Target->GetCode_IndirectlyAccessiblePropertyPtrDeclarations()));
	ReplicatorCode.HeadCode = FString::Format(bIsBlueprint ? CodeGen_BP_HeadCodeTemplate : CodeGen_CPP_HeadCodeTemplate, FormatArgs);
	ReplicatorCode.HeadFileName = Target->GetReplicatorClassName(false) + CodeGen_HeadFileExtension;
	// ---------- Head code ----------

	// ---------- Cpp code ----------
	FStringBuilderBase CppCodeBuilder;
	CppCodeBuilder.Append(TEXT("#include \"") + ReplicatorCode.HeadFileName + TEXT("\"\n\n"));
	FormatArgs.Add(
		TEXT("Code_AssignPropertyPointers"),
		FStringFormatArg(Target->GetCode_AssignPropertyPointers(TargetInstanceRef))
	);
	CppCodeBuilder.Append(FString::Format(
		bIsBlueprint ? CodeGen_BP_ConstructorImplTemplate : CodeGen_CPP_ConstructorImplTemplate,
		FormatArgs
	));
	CppCodeBuilder.Append(FString::Format(CodeGen_CPP_DestructorImplTemplate, FormatArgs));
	CppCodeBuilder.Append(FString::Format(CodeGen_CPP_GetDeltaStateImplTemplate, FormatArgs));
	CppCodeBuilder.Append(FString::Format(CodeGen_CPP_ClearStateImplTemplate, FormatArgs));

	FormatArgs.Add(
		TEXT("Code_AllPropertiesSetDeltaState"),
		Target->GetCode_AllPropertiesSetDeltaState(TargetInstanceRef,TEXT("FullState"), TEXT("DeltaState"))
	);
	CppCodeBuilder.Append(FString::Format(CodeGen_CPP_TickImplTemplate, FormatArgs));

	FormatArgs.Add(
		TEXT("Code_AllPropertyOnStateChanged"),
		Target->GetCode_AllPropertiesOnStateChange(TargetInstanceRef, TEXT("NewState"))
	);
	CppCodeBuilder.Append(FString::Format(CodeGen_CPP_OnStateChangedImplTemplate, FormatArgs));

	ReplicatorCode.CppCode = CppCodeBuilder.ToString();
	ReplicatorCode.CppFileName = Target->GetReplicatorClassName(false) + CodeGen_CppFileExtension;
	// ---------- Cpp code ----------

	// ---------- Protobuf ----------
	FStringFormatNamedArguments ProtoFormatArgs;
	ProtoFormatArgs.Add(TEXT("Declare_ProtoPackageName"), Target->GetProtoPackageName());
	ProtoFormatArgs.Add(TEXT("Definition_ProtoStateMsg"), Target->GetDefinition_ProtoStateMessage());
	ReplicatorCode.ProtoDefinitions = FString::Format(CodeGen_CPP_ProtoTemplate, ProtoFormatArgs);
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
		}
	}
}
