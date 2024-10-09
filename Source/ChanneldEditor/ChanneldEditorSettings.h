#pragma once

#include "CoreMinimal.h"
#include "GameFramework/ChanneldWorldSettings.h"
#include "View/ChannelDataView.h"
#include "Replication/ChanneldReplicationComponent.h"
#include "ChanneldEditorSettings.generated.h"

UCLASS(config = EditorPerProjectUserSettings)
class UChanneldEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	// The go source folder to build and run. The working directory is %CHANNELD_PATH%.
	UPROPERTY(Config, EditAnywhere, Category= "Channeld")
	FString LaunchChanneldEntry = TEXT("examples/channeld-ue-tps");

	// The parameters to run channeld. The working directory is %CHANNELD_PATH%.
	UPROPERTY(Config, EditAnywhere, Category = "Channeld")
	TArray<FString> LaunchChanneldParameters = {
		TEXT("-dev"),
		TEXT("-loglevel=-1"),
		TEXT("-mcb=13"),
		TEXT("-cfsm=config/client_authoratative_fsm.json"),
		TEXT("-chs=config/channel_settings_ue.json"),
		TEXT("-scc=config/spatial_static_2x2.json")
	};

	UPROPERTY(Config, EditAnywhere, Category = "Server")
	TArray<FServerLaunchGroup> ServerGroups;

	//Using to add the replication component to all replicated blueprint actors
	UPROPERTY(Config, EditAnywhere, Category = "Tools")
	TSubclassOf<UChanneldReplicationComponent> DefaultReplicationComponent = UChanneldReplicationComponent::StaticClass();

	// Replication Generator will automatically recompile the game code after generating the replicators. if you want to disable this feature, set the item to be false
	UPROPERTY(Config, EditAnywhere, Category = "Replication Generator", DisplayName = "Automatically Recompile After Generating Replication Code")
	bool bEnableCompatibleRecompilation = true;

	// Set go_package in generated .proto file to the specified value
	UPROPERTY(Config, EditAnywhere, Category = "Replication Generator|Protobuf", DisplayName= "Go Package Import Path Prefix")
	FString ChanneldGoPackageImportPathPrefix = TEXT("github.com/channeldorg/channeld/examples/channeld-ue-tps");

	// If developer wants to export replicators from the default game module, set the item to be the same as the default game module API Macro. For Example "CHANNELDUE_API"
	UPROPERTY(Config, EditAnywhere, Category = "Replication Generator|Protobuf", DisplayName = "Game Module Export API Macro")
	FString GameModuleExportAPIMacro;

	/*
    static int32 GetServerNum()
    {
        auto Settings = GetDefault<UChanneldEditorSettings>();
        return Settings->ServerNum;
    }

    static void SetServerNum(int32 InNum)
    {
        auto Settings = GetMutableDefault<UChanneldEditorSettings>();
        Settings->ServerNum = InNum;

        Settings->PostEditChange();
        Settings->SaveConfig();
    }

	static FText GetServerMapName()
	{
		auto Settings = GetDefault<UChanneldEditorSettings>();
		return Settings->ServerMapName;
	}

	static void SetServerMapName(const FText& InName)
	{
		auto Settings = GetMutableDefault<UChanneldEditorSettings>();
		Settings->ServerMapName = InName;

		Settings->PostEditChange();
		Settings->SaveConfig();
	}

	static FText GetAdditionalArgs()
	{
		auto Settings = GetDefault<UChanneldEditorSettings>();
		return Settings->AdditionalArgs;
	}

	static void SetAdditionalArgs(const FText& InArgs)
	{
		auto Settings = GetMutableDefault<UChanneldEditorSettings>();
		Settings->AdditionalArgs = InArgs;

		Settings->PostEditChange();
		Settings->SaveConfig();
	}
	*/
};
