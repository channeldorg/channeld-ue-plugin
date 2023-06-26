#include "ChanneldUE.h"
#include "Developer/Settings/Public/ISettingsModule.h"
#include "ChanneldSettings.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Replication/ChanneldReplication.h"
#include "Replication/ChanneldActorComponentReplicator.h"
#include "Replication/ChanneldSceneComponentReplicator.h"
#include "Replication/ChanneldCharacterReplicator.h"
#include "Replication/ChanneldControllerReplicator.h"
#include "Replication/ChanneldPlayerControllerReplicator.h"
#include "Replication/ChanneldPlayerStateReplicator.h"
#include "Replication/ChanneldGameStateBaseReplicator.h"
#include "Replication/ChanneldActorReplicator.h"
#include "Replication/ChanneldObjectReplicator.h"
#include "Replication/ChanneldPawnReplicator.h"

#define LOCTEXT_NAMESPACE "FChanneldUEModule"

void FChanneldUEModule::StartupModule()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "ChanneldSettings",
			LOCTEXT("RuntimeSettingsName", "Channeld"),
			LOCTEXT("RuntimeSettingsDesc", ""),
			GetMutableDefault<UChanneldSettings>());
	}

	REGISTER_REPLICATOR_SINGLETON(FChanneldGameStateBaseReplicator, AGameStateBase);
	REGISTER_REPLICATOR(FChanneldObjectReplicator, UObject);
	REGISTER_REPLICATOR(FChanneldActorReplicator, AActor);
	REGISTER_REPLICATOR(FChanneldPawnReplicator, APawn);
	REGISTER_REPLICATOR(FChanneldCharacterReplicator, ACharacter);
	REGISTER_REPLICATOR(FChanneldPlayerStateReplicator, APlayerState);
	REGISTER_REPLICATOR(FChanneldControllerReplicator, AController);
	REGISTER_REPLICATOR(FChanneldPlayerControllerReplicator, APlayerController);
	REGISTER_REPLICATOR(FChanneldActorComponentReplicator, UActorComponent);
	REGISTER_REPLICATOR(FChanneldSceneComponentReplicator, USceneComponent);

	SpatialChannelDataProcessor = new FDefaultSpatialChannelDataProcessor();
	ChanneldReplication::RegisterChannelDataProcessor(TEXT("unrealpb.SpatialChannelData"), SpatialChannelDataProcessor);
}

void FChanneldUEModule::ShutdownModule()
{
	delete SpatialChannelDataProcessor;
	
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "ChanneldSettings");
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FChanneldUEModule, ChanneldUE)
