// Fill out your copyright notice in the Description page of Project Settings.


#include "ChanneldCharacter.h"

#include "ChanneldCharMoveComponent.h"

AChanneldCharacter::AChanneldCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UChanneldCharMoveComponent>(CharacterMovementComponentName))
{
}
