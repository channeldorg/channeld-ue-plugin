#include "ReplicatorCodeGenerator.h"

bool FReplicatorCodeGenerator::GenerateCode(UClass* TargetActor, FReplicatorCodeGroup& ReplicatorCodeGroup, FString& ResultMessage)
{
	const TSharedPtr<FReplicatedActorDecorator> Target = MakeShareable(new FReplicatedActorDecorator(TargetActor));
	ReplicatorCodeGroup.Target = Target;

	const FString TargetInstanceRef = TEXT("Actor");
	FStringFormatNamedArguments FormatArgs;
	FormatArgs.Add(TEXT("Declare_ReplicatorClassName"), FStringFormatArg(Target->GetReplicatorClassName()));
	FormatArgs.Add(TEXT("Declare_TargetClassName"), FStringFormatArg(Target->GetActorCppClassName()));
	FormatArgs.Add(TEXT("Ref_TargetInstanceRef"), FStringFormatArg(TargetInstanceRef));
	FormatArgs.Add(TEXT("Declare_ProtoNamespace"), FStringFormatArg(Target->GetProtoNamespace()));
	FormatArgs.Add(TEXT("Declare_ProtoStateMsgName"), FStringFormatArg(Target->GetProtoStateMessageType()));
	FormatArgs.Add(TEXT("File_ProtoPbHead"), FStringFormatArg(Target->GetReplicatorClassName(false) + CodeGen_ProtoPbHeadExtension));

	// ---------- Head code ----------
	ReplicatorCodeGroup.HeadCode = FString::Format(CodeGen_HeadCodeTemplate, FormatArgs);
	ReplicatorCodeGroup.HeadFileName = Target->GetReplicatorClassName(false) + CodeGen_HeadFileExtension;
	// ---------- Head code ----------

	// ---------- Cpp code ----------
	FStringBuilderBase CppCodeBuilder;
	CppCodeBuilder.Append(TEXT("#include \"") + ReplicatorCodeGroup.HeadFileName + TEXT("\"\n\n"));
	CppCodeBuilder.Append(FString::Format(CodeGen_ConstructorImplTemplate, FormatArgs));
	CppCodeBuilder.Append(FString::Format(CodeGen_DestructorImplTemplate, FormatArgs));
	CppCodeBuilder.Append(FString::Format(CodeGen_GetDeltaStateImplTemplate, FormatArgs));
	CppCodeBuilder.Append(FString::Format(CodeGen_ClearStateImplTemplate, FormatArgs));

	FormatArgs.Add(
		TEXT("Code_AllPropertiesSetDeltaState"),
		Target->GetCode_AllPropertiesSetDeltaState(TargetInstanceRef,TEXT("FullState"), TEXT("DeltaState"))
	);
	CppCodeBuilder.Append(FString::Format(CodeGen_TickImplTemplate, FormatArgs));

	FormatArgs.Add(
		TEXT("Code_AllPropertyOnStateChanged"),
		Target->GetCode_AllPropertiesOnStateChange(TargetInstanceRef, TEXT("NewState"))
	);
	CppCodeBuilder.Append(FString::Format(CodeGen_OnStateChangedImplTemplate, FormatArgs));

	ReplicatorCodeGroup.CppCode = CppCodeBuilder.ToString();
	ReplicatorCodeGroup.CppFileName = Target->GetReplicatorClassName(false) + CodeGen_CppFileExtension;
	// ---------- Cpp code ----------

	// ---------- Protobuf ----------
	FStringFormatNamedArguments ProtoFormatArgs;
	ProtoFormatArgs.Add(TEXT("Declare_ProtoPackageName"), Target->GetProtoPackageName());
	ProtoFormatArgs.Add(TEXT("Definition_ProtoStateMsg"), Target->GetDefinition_ProtoStateMessage());
	ReplicatorCodeGroup.ProtoDefinitions = FString::Format(CodeGen_ProtoTemplate, ProtoFormatArgs);
	ReplicatorCodeGroup.ProtoFileName = Target->GetReplicatorClassName(false) + CodeGen_ProtoFileExtension;
	// ---------- Protobuf ----------

	return true;
}
