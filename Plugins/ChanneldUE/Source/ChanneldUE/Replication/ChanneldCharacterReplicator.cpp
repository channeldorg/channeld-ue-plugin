#include "ChanneldCharacterReplicator.h"
#include "Net/UnrealNetwork.h"
#include "Engine/PackageMapClient.h"
#include "ChanneldReplicationComponent.h"

FChanneldCharacterReplicator::FChanneldCharacterReplicator(UObject* InTargetObj) : FChanneldReplicatorBase(InTargetObj)
{
	Character = CastChecked<ACharacter>(InTargetObj);
	// Remove the registered DOREP() properties in the SceneComponent
	TArray<FLifetimeProperty> RepProps;
	DisableAllReplicatedPropertiesOfClass(Character->GetClass(), ACharacter::StaticClass(), EFieldIteratorFlags::ExcludeSuper, RepProps);
}

FChanneldCharacterReplicator::~FChanneldCharacterReplicator()
{

}

google::protobuf::Message* FChanneldCharacterReplicator::GetState()
{
	return State;
}

void FChanneldCharacterReplicator::ClearState()
{

}

void FChanneldCharacterReplicator::Tick(float DeltaTime)
{

}

void FChanneldCharacterReplicator::OnStateChanged(const google::protobuf::Message* NewState)
{

}
