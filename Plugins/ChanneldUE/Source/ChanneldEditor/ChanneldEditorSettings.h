#pragma once

#include "CoreMinimal.h"
#include "View/ChannelDataView.h"
#include "Replication/ChanneldReplicationComponent.h"
#include "ChanneldEditorSettings.generated.h"

USTRUCT()
struct FServerGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "1", ClampMax = "16"))
	int32 ServerNum = 1;

	// How long to wait before launching the servers (in seconds)
	UPROPERTY(EditAnywhere)
	float DelayTime;

	// If not set, the open map in the Editor will be used.
	UPROPERTY(EditAnywhere, meta=(AllowedClasses="World"))
	FSoftObjectPath ServerMap;

	// If not set, the ChannelDataViewClass in the UChanneldSettings will be used.
	UPROPERTY(EditAnywhere)
	TSubclassOf<UChannelDataView> ServerViewClass;

	UPROPERTY(EditAnywhere)
	FText AdditionalArgs;

	FTimerHandle DelayHandle;
};

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
	TArray<FServerGroup> ServerGroups;

	//Using to add the replication component to all replicated blueprint actors
	UPROPERTY(Config, EditAnywhere, Category = "Tools")
	TSubclassOf<UChanneldReplicationComponent> DefaultReplicationComponent = UChanneldReplicationComponent::StaticClass();

	// Replication Generator will automatically recompile the game code after generating the replicators. if you want to disable this feature, set the item to be false
	UPROPERTY(Config, EditAnywhere, Category = "Replication Generator", DisplayName = "Automatically Recompile After Generating Replication Code")
	bool bAutoRecompileAfterGenerate = true;

	// Generate go replication code to the specified folder after generating replication code, the folder is relative to "CHANNELD_PATH"
	UPROPERTY(Config, EditAnywhere, Category = "Replication Generator")
	FString GeneratedGoReplicationCodeStorageFolder = TEXT("examples/channeld-ue-tps");

	// Set go_package in generated .proto file to the specified value
	UPROPERTY(Config, EditAnywhere, Category = "Replication Generator|Protobuf", DisplayName= "Go Package Import Path Prefix")
	FString ChanneldGoPackageImportPathPrefix = TEXT("channeld.clewcat.com/channeld/examples/channeld-ue-tps");

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
