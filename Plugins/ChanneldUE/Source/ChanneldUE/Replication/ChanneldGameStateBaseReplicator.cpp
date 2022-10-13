#include "ChanneldGameStateBaseReplicator.h"
#include "Net/UnrealNetwork.h"
#include "ChanneldUtils.h"
#include "GameFramework/SpectatorPawn.h"
#include "Kismet/KismetSystemLibrary.h"

FChanneldGameStateBaseReplicator::FChanneldGameStateBaseReplicator(UObject* InTargetObj) : FChanneldReplicatorBase(InTargetObj)
{
	GameStateBase = CastChecked<AGameStateBase>(InTargetObj);
	// Remove the registered DOREP() properties in the Character
	TArray<FLifetimeProperty> RepProps;
	DisableAllReplicatedPropertiesOfClass(InTargetObj->GetClass(), AGameStateBase::StaticClass(), EFieldIteratorFlags::ExcludeSuper, RepProps);

	FullState = new unrealpb::GameStateBase;
	DeltaState = new unrealpb::GameStateBase;

	// Prepare Reflection pointers
	{
		auto Property = CastFieldChecked<const FFloatProperty>(GameStateBase->GetClass()->FindPropertyByName(FName("ReplicatedWorldTimeSeconds")));
		ReplicatedWorldTimeSecondsPtr = Property->ContainerPtrToValuePtr<float>(GameStateBase.Get());
		check(ReplicatedWorldTimeSecondsPtr);
	}
	{
		auto Property = CastFieldChecked<const FBoolProperty>(GameStateBase->GetClass()->FindPropertyByName(FName("bReplicatedHasBegunPlay")));
		bReplicatedHasBegunPlayPtr = Property->ContainerPtrToValuePtr<bool>(GameStateBase.Get());
		check(bReplicatedHasBegunPlayPtr);
	}
	OnRep_ReplicatedWorldTimeSecondsFunc = GameStateBase->GetClass()->FindFunctionByName(FName("OnRep_ReplicatedWorldTimeSeconds"));
	check(OnRep_ReplicatedWorldTimeSecondsFunc);
	OnRep_ReplicatedHasBegunPlayFunc = GameStateBase->GetClass()->FindFunctionByName(FName("OnRep_ReplicatedHasBegunPlay"));
	check(OnRep_ReplicatedHasBegunPlayFunc);
}

FChanneldGameStateBaseReplicator::~FChanneldGameStateBaseReplicator()
{
	delete FullState;
	delete DeltaState;
}

uint32 FChanneldGameStateBaseReplicator::GetNetGUID()
{
	// GameStateBase doesn't have a valid NetGUID, so let's use a constant value.
	return 1;
}

google::protobuf::Message* FChanneldGameStateBaseReplicator::GetDeltaState()
{
	return DeltaState;
}

void FChanneldGameStateBaseReplicator::ClearState()
{
	DeltaState->Clear();
	bStateChanged = false;
}

void FChanneldGameStateBaseReplicator::Tick(float DeltaTime)
{
	if (!GameStateBase.IsValid())
	{
		return;
	}

	// Only server can update channel data
	if (!GameStateBase->HasAuthority())
	{
		return;
	}

	auto SpectatorClass = LoadClass<ASpectatorPawn>(NULL, UTF8_TO_TCHAR(FullState->mutable_spectatorclassname()->c_str()));
	if (GameStateBase->SpectatorClass != SpectatorClass)
	{
		DeltaState->set_spectatorclassname(std::string(TCHAR_TO_UTF8(*GameStateBase->SpectatorClass->GetPathName())));
		bStateChanged = true;
	}

	auto GameModeClass = LoadClass<AGameModeBase>(NULL, UTF8_TO_TCHAR(FullState->mutable_gamemodeclassname()->c_str()));
	if (GameStateBase->GameModeClass != GameModeClass)
	{
		DeltaState->set_gamemodeclassname(std::string(TCHAR_TO_UTF8(*GameStateBase->GameModeClass->GetPathName())));
		bStateChanged = true;
	}

	if (*ReplicatedWorldTimeSecondsPtr != FullState->replicatedworldtimeseconds())
	{
		DeltaState->set_replicatedworldtimeseconds(*ReplicatedWorldTimeSecondsPtr);
		bStateChanged = true;
	}
	if (*bReplicatedHasBegunPlayPtr != FullState->breplicatedhasbegunplay())
	{
		DeltaState->set_breplicatedhasbegunplay(*bReplicatedHasBegunPlayPtr);
		bStateChanged = true;
	}

	FullState->MergeFrom(*DeltaState);
}

void FChanneldGameStateBaseReplicator::OnStateChanged(const google::protobuf::Message* InNewState)
{
	if (!GameStateBase.IsValid())
	{
		return;
	}

	//if (!UKismetSystemLibrary::IsServer(GameStateBase.Get()))
	//{
	//	GameStateBase->SetRole(ROLE_SimulatedProxy);
	//}

	auto NewState = static_cast<const unrealpb::GameStateBase*>(InNewState);
	FullState->MergeFrom(*NewState);
	bStateChanged = false;

	if (NewState->has_spectatorclassname())
	{
		GameStateBase->SpectatorClass = LoadClass<ASpectatorPawn>(NULL, UTF8_TO_TCHAR(NewState->spectatorclassname().c_str()));
		GameStateBase->ReceivedSpectatorClass();
	}

	if (NewState->has_gamemodeclassname())
	{
		GameStateBase->GameModeClass = LoadClass<AGameModeBase>(NULL, UTF8_TO_TCHAR(NewState->gamemodeclassname().c_str()));
		GameStateBase->ReceivedGameModeClass();
	}

	if (NewState->has_replicatedworldtimeseconds())
	{
		*ReplicatedWorldTimeSecondsPtr = NewState->replicatedworldtimeseconds();
		GameStateBase->ProcessEvent(OnRep_ReplicatedWorldTimeSecondsFunc, NULL);
	}
	if (NewState->has_breplicatedhasbegunplay())
	{
		*bReplicatedHasBegunPlayPtr = NewState->breplicatedhasbegunplay();
		GameStateBase->ProcessEvent(OnRep_ReplicatedHasBegunPlayFunc, NULL);
	}
}

