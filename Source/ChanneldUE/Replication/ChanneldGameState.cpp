#include "ChanneldGameState.h"

int32 AChanneldGameState::GetFunctionCallspace(UFunction* Function, FFrame* Stack)
{
	//~ Begin copy of AActor::GetFunctionCallspace
	if (GAllowActorScriptExecutionInEditor)
	{
		return FunctionCallspace::Local;
	}

	if ((Function->FunctionFlags & FUNC_Static) || (GetWorld() == nullptr))
	{
		return GEngine->GetGlobalFunctionCallspace(Function, this, Stack);
	}
	//~ End copy of AActor::GetFunctionCallspace

	// Invoking on server with no authority - sends cross-server RPC to the server tha owns the object.
	if ((Function->FunctionFlags & FUNC_NetServer) && GetNetMode() == NM_DedicatedServer && GetLocalRole() != ROLE_Authority)
	{
		return FunctionCallspace::Remote | FunctionCallspace::Local;
	}
	
	return Super::GetFunctionCallspace(Function, Stack);
}
