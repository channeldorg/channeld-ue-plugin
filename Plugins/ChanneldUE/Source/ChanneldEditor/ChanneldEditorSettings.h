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
	bool bEnabled;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "1", ClampMax = "16"))
	int32 ServerNum;

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
	UPROPERTY(Config, EditAnywhere, Category= "Channeld", meta = ( ToolTip = "The go source directory or file to build and run. The working directory is CHANNELD_PATH." ))
	FString LaunchChanneldEntry = TEXT("examples/channeld-ue-tps/main.go");

	UPROPERTY(Config, EditAnywhere, Category = "Channeld")
	FString LaunchChanneldParameters = TEXT("-cfsm=\"config/client_authoratative_fsm.json\" -dev -loglevel=-1 -ct=0 -mcb=13 -scc=\"config/spatial_static_2x2.json\"");

	UPROPERTY(Config, EditAnywhere, Category = "Channeld")
	TArray<FServerGroup> ServerGroups;

	UPROPERTY(Config, EditAnywhere, Category = "Replicator Generator", DisplayName = "Automatically Recompile After Generate Replicators", meta = ( ToolTip = "Repilcator Generator will automatically recompile the game code after generating the replicators. if you want to disable this feature, set the item to be false" ))
	bool bAutoRecompileAfterGenerate = true;

	UPROPERTY(Config, EditAnywhere, Category = "Replicator Generator|Protobuf", DisplayName = "Game Module Export API Macro", meta = ( ToolTip = "If developer wants to export replicators from the default game module, set the item to be the same as the default game module API Macro. For Example \"CHANNELDUE_API\"" ))
	FString GameModuleExportAPIMacro;

	UPROPERTY(Config, EditAnywhere, Category = "Replicator Generator|Protobuf", meta = ( ToolTip = "Set go_package in generated .proto file to the specified value" ))
	FString ChanneldGoPackage = TEXT("channeld.clewcat.com/channeld/examples/channeld-ue-tps/channeldgeneratedpb");
	
	UPROPERTY(Config, EditAnywhere, Category = "Replicator Generator|Protobuf", DisplayName = "Channeld Proto Files Storage Directory", meta = ( ToolTip = "Copy the .proto files to the specified directory after generating replicators, the directory is relative to CHANNELD_PATH" ))
	FString ChanneldProtoFilesStorageDir = TEXT("examples/channeld-ue-tps/channeldgeneratedpb");

	UPROPERTY(Config, EditAnywhere, Category = "Replicator Generator|Tools", DisplayName = "Default Replication Component", meta = ( ToolTip = "Using to add the replicator component to all replicated blueprint actors without replicator" ))
	TSubclassOf<UChanneldReplicationComponent> DefaultReplicationComponent;
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
