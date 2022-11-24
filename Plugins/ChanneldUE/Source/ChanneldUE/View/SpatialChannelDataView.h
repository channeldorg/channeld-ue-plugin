// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "View/ChannelDataView.h"
#include "ChanneldNetConnection.h"
#include "PlayerStartLocator.h"
#include "SpatialChannelDataView.generated.h"

/**
 * 
 */
UCLASS()
class CHANNELDUE_API USpatialChannelDataView : public UChannelDataView
{
	GENERATED_BODY()

public:
	USpatialChannelDataView(const FObjectInitializer& ObjectInitializer);
	
	virtual void InitServer() override;
	virtual void InitClient() override;

	virtual bool GetSendToChannelId(UChanneldNetConnection* NetConn, uint32& OutChId) const override;
	
	virtual void AddProvider(ChannelId ChId, IChannelDataProvider* Provider) override;
	virtual void AddProviderToDefaultChannel(IChannelDataProvider* Provider) override;
	virtual void RemoveProvider(ChannelId ChId, IChannelDataProvider* Provider, bool bSendRemoved) override;

	virtual void OnClientPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer, UChanneldNetConnection* NewPlayerConn) override;
	virtual void OnSpawnedObject(UObject* Obj, const FNetworkGUID NetId, ChannelId ChId) override;
	
private:

    // Map the client to the channels, so the spatial server's LowLevelSend() can use the right channelId.
	TMap<uint32, ChannelId> ClientInChannels;
	
	bool bClientInMasterServer = false;

	// Use by the server to locate the player start position. In order to spawn the player's pawn in the right spatial channel,
	// the Master server and spatial servers should have the EXACTLY SAME position for a player.
	UPROPERTY()
	UPlayerStartLocatorBase* PlayerStartLocator;

	void ClientHandleSubToChannel(UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg);

	UChanneldNetConnection* CreateClientConnection(ConnectionId ConnId, ChannelId ChId);
};
