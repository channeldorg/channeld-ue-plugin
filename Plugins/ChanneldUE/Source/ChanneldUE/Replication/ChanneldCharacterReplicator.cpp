#include "ChanneldCharacterReplicator.h"
#include "Net/UnrealNetwork.h"

FChanneldCharacterReplicator::FChanneldCharacterReplicator(ACharacter* InCharacter)
{
    Character = InCharacter;

	// Remove the registered DOREP() properties in the SceneComponent
	TArray<FLifetimeProperty> RepProps;
	DisableAllReplicatedPropertiesOfClass(InCharacter->GetClass(), ACharacter::StaticClass(), EFieldIteratorFlags::ExcludeSuper, RepProps);
}

FChanneldCharacterReplicator::~FChanneldCharacterReplicator()
{
    
}