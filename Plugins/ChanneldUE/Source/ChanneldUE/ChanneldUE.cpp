#include "ChanneldUE.h"
#include "ISettingsModule.h"
#include "ChanneldSettings.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Character.h"
#include "Replication/ChanneldReplicatorBase.h"
#include "Replication/ChanneldCharacterReplicator.h"
#include "Replication/ChanneldSceneComponentReplicator.h"
#include "Replication/ChanneldReplication.h"

#define LOCTEXT_NAMESPACE "FChanneldUEModule"

void FChanneldUEModule::StartupModule()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "ChanneldSettings",
			LOCTEXT("RuntimeSettingsName", "Channeld Settings"), 
			LOCTEXT("RuntimeSettingsDesc", ""),
			GetMutableDefault<UChanneldSettings>());
	}

	REGISTER_REPLICATOR(FChanneldSceneComponentReplicator, USceneComponent);
	REGISTER_REPLICATOR(FChanneldCharacterReplicator, ACharacter);
}

void FChanneldUEModule::ShutdownModule()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "ChanneldSettings");
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FChanneldUEModule, ChanneldUE)