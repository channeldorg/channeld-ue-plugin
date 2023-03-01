#pragma once
#include "PropertyDecorator.h"
#include "PropertyDecorator/StructPropertyDecorator.h"

static const TCHAR* RPC_SerializeFuncParamsTemp =
	LR"EOF(
if(Func->GetFName() == FName("{Declare_FuncName}"))
{
  {Declare_ParamStructNamespace}::{Declare_PropPtrGroupStructName} ParamPointerGroup(Params, OutParams);
  auto Msg = new {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}();
  ParamPointerGroup.Merge(nullptr, Msg, {Code_GetWorldRef});
  return MakeShareable(Msg);
}
)EOF";

static const TCHAR* RPC_DeserializeFuncParamsTemp =
	LR"EOF(
if(Func->GetFName() == FName("{Declare_FuncName}"))
{
  {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName} Msg;
  if (!Msg.ParseFromString(ParamsPayload))
  {
    UE_LOG(LogChanneldGen, Warning, TEXT("Failed to parse {Declare_FuncName} Params"));
    return nullptr;
  }
  {Declare_ParamStructNamespace}::{Declare_ParamStructCopy}* Params = new {Declare_ParamStructNamespace}::{Declare_ParamStructCopy}();
  {Declare_ParamStructNamespace}::{Declare_PropPtrGroupStructName}::SetPropertyValue(Params, &Msg, {Code_GetWorldRef});

  return MakeShareable(Params);
}
)EOF";

class FRPCDecorator : public FStructPropertyDecorator
{
public:
	FRPCDecorator(UFunction*, IPropertyDecoratorOwner*);

	virtual ~FRPCDecorator() = default;

	virtual bool Init(const TFunction<FString()>& SetNameForIllegalPropName) override;
	
	virtual bool IsDirectlyAccessible() override;

	virtual FString GetCPPType() override;
	
	virtual FString GetProtoPackageName() override;
	virtual FString GetProtoNamespace() override;
	virtual FString GetProtoStateMessageType() override;

	FString GetCode_SerializeFunctionParams();
	FString GetCode_DeserializeFunctionParams();
	FString GetDeclaration_ProtoFields();
	
	virtual TArray<FString> GetAdditionalIncludes() override;

	virtual FString GetCode_GetWorldRef() override;

protected:
	FReplicatedActorDecorator* OwnerActor;
	UFunction* OriginalFunction;
	FName FunctionName;
};
