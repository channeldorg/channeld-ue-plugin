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
#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 2)
		auto Property = CastFieldChecked<const FFloatProperty>(GameStateBase->GetClass()->FindPropertyByName(FName("ReplicatedWorldTimeSeconds")));
		ReplicatedWorldTimeSecondsPtr = Property->ContainerPtrToValuePtr<float>(GameStateBase.Get());
#else
		auto Property = CastFieldChecked<const FDoubleProperty>(GameStateBase->GetClass()->FindPropertyByName(FName("ReplicatedWorldTimeSecondsDouble")));
		ReplicatedWorldTimeSecondsPtr = Property->ContainerPtrToValuePtr<double>(GameStateBase.Get());
#endif
		check(ReplicatedWorldTimeSecondsPtr);
	}
	{
		auto Property = CastFieldChecked<const FBoolProperty>(GameStateBase->GetClass()->FindPropertyByName(FName("bReplicatedHasBegunPlay")));
		bReplicatedHasBegunPlayPtr = Property->ContainerPtrToValuePtr<bool>(GameStateBase.Get());
		check(bReplicatedHasBegunPlayPtr);
	}
	OnRep_GameModeClassFunc = GameStateBase->GetClass()->FindFunctionByName(FName("OnRep_GameModeClass"));
	check(OnRep_GameModeClassFunc);
	OnRep_SpectatorClassFunc = GameStateBase->GetClass()->FindFunctionByName(FName("OnRep_SpectatorClass"));
	check(OnRep_SpectatorClassFunc);

#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 2)
	OnRep_ReplicatedWorldTimeSecondsFunc = GameStateBase->GetClass()->FindFunctionByName(FName("OnRep_ReplicatedWorldTimeSeconds"));
#else
	OnRep_ReplicatedWorldTimeSecondsFunc = GameStateBase->GetClass()->FindFunctionByName(FName("OnRep_ReplicatedWorldTimeSecondsDouble"));
#endif
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
	// GameState doesn't have a valid NetGUID, so let's use a constant value.
	return Channeld::GameStateNetId;
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

	// For LoadClass(), check if the class name is set before calling can avoid warning
	TSubclassOf<ASpectatorPawn> SpectatorClass = FullState->has_spectatorclassname() ? LoadClass<ASpectatorPawn>(NULL, UTF8_TO_TCHAR(FullState->spectatorclassname().c_str())) : NULL;
	if (GameStateBase->SpectatorClass != SpectatorClass)
	{
		DeltaState->set_spectatorclassname(std::string(TCHAR_TO_UTF8(*GameStateBase->SpectatorClass->GetPathName())));
		bStateChanged = true;
	}

	TSubclassOf<AGameModeBase> GameModeClass = FullState->has_gamemodeclassname() ? LoadClass<AGameModeBase>(NULL, UTF8_TO_TCHAR(FullState->gamemodeclassname().c_str())) : NULL;
	if (GameStateBase->GameModeClass != GameModeClass)
	{
		DeltaState->set_gamemodeclassname(std::string(TCHAR_TO_UTF8(*GameStateBase->GameModeClass->GetPathName())));
		bStateChanged = true;
	}

	if (!FMath::IsNearlyEqual((double)*ReplicatedWorldTimeSecondsPtr, FullState->replicatedworldtimeseconds(), 5.0))
	{
		DeltaState->set_replicatedworldtimeseconds(*ReplicatedWorldTimeSecondsPtr);
		bStateChanged = true;
	}
	if (*bReplicatedHasBegunPlayPtr != FullState->breplicatedhasbegunplay())
	{
		DeltaState->set_breplicatedhasbegunplay(*bReplicatedHasBegunPlayPtr);
		bStateChanged = true;
	}

	if (bStateChanged)
	{
		FullState->MergeFrom(*DeltaState);
	}
}

void FChanneldGameStateBaseReplicator::OnStateChanged(const google::protobuf::Message* InNewState)
{
	if (!GameStateBase.IsValid())
	{
		return;
	}

	// Only client needs to apply the new state
	if (GameStateBase->HasAuthority())
	{
		return;
	}

	auto NewState = static_cast<const unrealpb::GameStateBase*>(InNewState);
	FullState->MergeFrom(*NewState);
	bStateChanged = false;

	if (NewState->has_spectatorclassname())
	{
		GameStateBase->SpectatorClass = LoadClass<ASpectatorPawn>(NULL, UTF8_TO_TCHAR(NewState->spectatorclassname().c_str()));
	}
	if (NewState->has_gamemodeclassname())
	{
		GameStateBase->GameModeClass = LoadClass<AGameModeBase>(NULL, UTF8_TO_TCHAR(NewState->gamemodeclassname().c_str()));
	}
	if (NewState->has_replicatedworldtimeseconds())
	{
		*ReplicatedWorldTimeSecondsPtr = NewState->replicatedworldtimeseconds();
	}
	if (NewState->has_breplicatedhasbegunplay())
	{
		*bReplicatedHasBegunPlayPtr = NewState->breplicatedhasbegunplay();
	}

	if (NewState->has_spectatorclassname())
	{
		GameStateBase->ProcessEvent(OnRep_GameModeClassFunc, nullptr);
	}
	if (NewState->has_gamemodeclassname())
	{
		GameStateBase->ProcessEvent(OnRep_SpectatorClassFunc, nullptr);
	}
	if (NewState->has_replicatedworldtimeseconds())
	{
		GameStateBase->ProcessEvent(OnRep_ReplicatedWorldTimeSecondsFunc, nullptr);
	}
	if (NewState->has_breplicatedhasbegunplay())
	{
		GameStateBase->ProcessEvent(OnRep_ReplicatedHasBegunPlayFunc, nullptr);
	}
}

