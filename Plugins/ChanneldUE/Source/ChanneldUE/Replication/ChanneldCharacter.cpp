// Fill out your copyright notice in the Description page of Project Settings.


#include "ChanneldCharacter.h"

#include "ChanneldCharMoveComponent.h"
#include "ChanneldTypes.h"

AChanneldCharacter::AChanneldCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UChanneldCharMoveComponent>(CharacterMovementComponentName))
{
}

int32 AChanneldCharacter::GetFunctionCallspace(UFunction* Function, FFrame* Stack)
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

void AChanneldCharacter::PostNetReceiveLocationAndRotation()
{
	// Don't perform SmoothCorrection if we are a simulated proxy on a dedicated server, to avoid check crash.
	// This case should only exist when the server's interest border size > 0.
	if (GetNetMode() == NM_DedicatedServer && GetLocalRole() == ROLE_SimulatedProxy)
	{
		UE_LOG(LogChanneld, Log, TEXT("Skip ACharacter::PostNetReceiveLocationAndRotation for SimulatedProxy on dedicated server."))
		AActor::PostNetReceiveLocationAndRotation();
	}
	else
	{
		ACharacter::PostNetReceiveLocationAndRotation();
	}
}
