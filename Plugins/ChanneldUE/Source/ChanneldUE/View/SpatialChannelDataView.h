// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "View/ChannelDataView.h"
#include "ChanneldNetConnection.h"
#include "PlayerStartLocator.h"
#include "ProtoMessageObject.h"
#include "Tools/SpatialVisualizer.h"
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
	virtual bool OnServerSpawnedObject(UObject* Obj, const FNetworkGUID NetId) override;
	virtual void SendSpawnToConn(AActor* Actor, UChanneldNetConnection* NetConn, uint32 OwningConnId) override;

	UPROPERTY(EditAnywhere)
	UProtoMessageObject* ChannelInitData;
	
private:

    // Map the client to the channels, so the spatial server's LowLevelSend() can use the right channelId.
	TMap<uint32, ChannelId> ClientInChannels;
	
	bool bClientInMasterServer = false;

	// Use by the server to locate the player start position. In order to spawn the player's pawn in the right spatial channel,
	// the Master server and spatial servers should have the EXACTLY SAME position for a player.
	UPROPERTY()
	UPlayerStartLocatorBase* PlayerStartLocator;

	UPROPERTY()
	USpatialVisualizer* Visualizer;

	// Object that are spawned in the server but don't need to send the spawn to the client connection.
	// Common case: handover pawn should not be sent to the client that owns the pawn.
	TMap<TWeakObjectPtr<UObject>, ConnectionId> ServerIgnoreSendSpawnObjects;

	void ServerHandleHandover(UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg);
	void ClientHandleSubToChannel(UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg);
	void ClientHandleHandover(UChanneldConnection* _, ChannelId ChId, const google::protobuf::Message* Msg);

	/**
	 * @brief Create the ChanneldNetConnection for the client. The PlayerController will not be created for the connection.
	 * @param ConnId The channeld connection ID of the client
	 * @param ChId The spatial channel the client belongs to 
	 * @return 
	 */
	UChanneldNetConnection* CreateClientConnection(ConnectionId ConnId, ChannelId ChId);
	void CreatePlayerController(UChanneldNetConnection* ClientConn);

	void SendSpawnToAdjacentChannels(UObject* Obj, ChannelId SpatialChId);
};
