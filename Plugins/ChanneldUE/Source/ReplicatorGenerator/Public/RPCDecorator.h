#pragma once
#include "PropertyDecorator.h"
#include "PropertyDecorator/StructPropertyDecorator.h"

static const TCHAR* RPC_SerializeFuncParamsTemp =
	LR"EOF(
if(Func->GetFName() == FName("{Declare_FuncName}"))
{
  {Declare_PropPtrGroupStructName} ParamPointerGroup(Params, OutParams);
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
    UE_LOG(LogChanneld, Warning, TEXT("Failed to parse {Declare_FuncName} Params"));
    return nullptr;
  }
  {Declare_ParamStructCopy}* Params = new {Declare_ParamStructCopy}();
  {Declare_PropPtrGroupStructName}::SetPropertyValue(Params, &Msg, {Code_GetWorldRef});

  return MakeShareable(Params);
}
)EOF";

class FRPCDecorator : public FStructPropertyDecorator
{
public:
	FRPCDecorator(UFunction*, IPropertyDecoratorOwner*);

	virtual ~FRPCDecorator() = default;

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
	UFunction* OriginalFunction;
	FName FunctionName;
};
