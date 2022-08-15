
#pragma once

#include "CoreMinimal.h"
#include "ChanneldEditorSettings.generated.h"

UCLASS(config = ChanneldEditor)
class UChanneldEditorSettings : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(config, EditAnywhere, Category = "Channeld|Editor", meta = (ClampMin = "1", ClampMax = "16"))
        int32 ServerNum;

    UPROPERTY(config, EditAnywhere, Category = "Channeld|Editor")
        FText ServerMapName;

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
};