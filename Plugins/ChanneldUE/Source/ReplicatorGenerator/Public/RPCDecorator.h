#pragma once
#include "PropertyDecorator.h"
#include "PropertyDecorator/StructPropertyDecorator.h"

static const TCHAR* RPC_SerializeFuncParamsTemp =
	LR"EOF(
if(Func->GetFName() == FName("{Declare_FuncName}"))
{
  {Declare_PropPtrGroupStructName} ParamPointerGroup(Func->PropertyLink, Params);
  auto Msg = new {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}();
  ParamPointerGroup.SetStateValue(Msg);
  return MakeShareable(Msg);
}
)EOF";

static const TCHAR* RPC_DeserializeFuncParamsTemp =
	LR"EOF(
if(Func->GetFName() == FName("{Declare_FuncName}"))
{
  {Declare_PropPtrGroupStructName} ParamPointerGroup(Func->PropertyLink, Params);
  auto Msg = new {Declare_ProtoNamespace}::{Declare_ProtoStateMsgName}();
  ParamPointerGroup.SetStateValue(Msg);
  return MakeShareable(Msg);
}
)EOF";

class FRPCDecorator : public FStructPropertyDecorator
{
public:
	FRPCDecorator(UFunction*, IPropertyDecoratorOwner*);

	virtual ~FRPCDecorator() = default;

	virtual bool IsBlueprintType() override;
	virtual FString GetProtoPackageName() override;
	virtual FString GetProtoNamespace() override;
	virtual FString GetProtoStateMessageType() override;

	FString GetCode_SerializeFunctionParams();

protected:
	UFunction* OriginalFunction;
	FName FunctionName;
};
