
#pragma once

#include "CoreMinimal.h"
#include "ChannelDataView.h"
#include "MultiSubworldsView.generated.h"

class UChanneldConnection;

/**
 * 
 */
UCLASS()
class CHANNELDUE_API UMultiSubworldsView : public UChannelDataView
{
	GENERATED_BODY()
	
public:
	UMultiSubworldsView(const FObjectInitializer& ObjectInitializer);

	virtual void InitServer() override;
	virtual void InitClient() override;

	FString ChannelMetadata = TEXT("");
};
