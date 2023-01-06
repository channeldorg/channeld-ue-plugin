
#pragma once

#include "CoreMinimal.h"
#include "View/ChannelDataView.h"
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

	UPROPERTY(Config, EditAnywhere, Category = "Channeld")
	TArray<FServerGroup> ServerGroups;

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