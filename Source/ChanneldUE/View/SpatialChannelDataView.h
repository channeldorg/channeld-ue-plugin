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

	virtual Channeld::ChannelId GetOwningChannelId(AActor* Actor) const override;
	virtual void SetOwningChannelId(const FNetworkGUID NetId, Channeld::ChannelId ChId) override;
	virtual bool GetSendToChannelId(UChanneldNetConnection* NetConn, uint32& OutChId) const override;
	
	virtual void AddProviderToDefaultChannel(IChannelDataProvider* Provider) override;

	virtual void OnAddClientConnection(UChanneldNetConnection* ClientConnection, Channeld::ChannelId ChId) override;
	virtual void OnRemoveClientConnection(UChanneldNetConnection* ClientConn) override;
	virtual void OnClientPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer, UChanneldNetConnection* NewPlayerConn) override;
	virtual void OnNetSpawnedObject(UObject* Obj, const Channeld::ChannelId ChId) override;
	
	virtual bool OnServerSpawnedObject(UObject* Obj, const FNetworkGUID NetId) override;
	virtual void OnDestroyedActor(AActor* Actor, const FNetworkGUID NetId) override;
	virtual void SendSpawnToConn(UObject* Obj, UChanneldNetConnection* NetConn, uint32 OwningConnId) override;
	virtual void SendSpawnToClients(UObject* Obj, uint32 OwningConnId) override;
	virtual void SendDestroyToClients(UObject* Obj, const FNetworkGUID NetId) override;
	
	UPROPERTY(EditAnywhere)
	int GlobalChannelFanOutIntervalMs = 20;
	
	// Make the first fan-out of GLOBAL channel data update (GameStateBase) a bit slower, so the GameState is spawned from the spatial server and ready.
	UPROPERTY(EditAnywhere)
	int GlobalChannelFanOutDelayMs = 2000;

	UPROPERTY(EditAnywhere)
	UProtoMessageObject* ChannelInitData;

protected:
	virtual void ServerHandleClientUnsub(Channeld::ConnectionId ClientConnId, channeldpb::ChannelType ChannelType, Channeld::ChannelId ChId) override;

	virtual void SendSpawnToClients_EntityChannelReady(const FNetworkGUID NetId, UObject* Obj, uint32 OwningConnId, Channeld::ChannelId SpatialChId);
	virtual void SendSpawnToConn_EntityChannelReady(UObject* Obj, UChanneldNetConnection* NetConn, uint32 OwningConnId);
	// The client need to destroy the objects that are no longer relevant to the client.
	virtual void OnRemovedProvidersFromChannel(Channeld::ChannelId ChId, channeldpb::ChannelType ChannelType, const TSet<FProviderInternal>& RemovedProviders) override;
	bool ClientDeleteObject(UObject* Obj);

	virtual bool ConsumeChannelUpdateData(Channeld::ChannelId ChId, google::protobuf::Message* UpdateData) override;

	bool TryToResolveObjects(Channeld::ChannelId ChId, TArray<uint32> NetGUIDs);
	// The client may have subscribed to the spatial channels that go beyond the interest area of the client's authoritative server.
	// In that case, the client may receive ChannelDataUpdate that contains unresolved NetworkGUIDs, so it needs to spawn the objects before applying the update.
	virtual bool CheckUnspawnedObject(Channeld::ChannelId ChId, const google::protobuf::Message* ChannelData) override;
	
	/**
	 * @brief The source server decides which objects get handed over to the destination server.
	 * @param Obj The object that just moved across the server boundary, normally a Pawn.
	 * @param SrcChId  The channel the object was in, before the handover. Should be in the OwnedChannels of this server.
	 * @param DstChId The channel the object is going to be handed over to. Should NOT be in the OwnedChannels of this server.
	 * @return The group of objects that should be sent to channeld for handover. If empty, the handover will not happen.
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Spatial")
	TArray<UObject*> GetHandoverObjects(UObject* Obj, int32 SrcChId, int32 DstChId);

	// The NetId of objects that are contained in the channel data update but unresolved in the client.
	// The client needs to ask channeld for the full-exported UnrealObjectRef in order to create it.
	TSet<uint32> ResolvingNetGUIDs;
	
private:
	const FName GameplayerDebuggerClassName = FName("GameplayDebuggerCategoryReplicator");

    // [Server only] Map the client to the channels, so the spatial server's LowLevelSend() can use the right channelId.
	TMap<Channeld::ConnectionId, Channeld::ChannelId> ClientInChannels;
	
	bool bClientInMasterServer = false;

	// Use by the server to locate the player start position. In order to spawn the player's pawn in the right spatial channel,
	// the Master server and spatial servers should have the EXACTLY SAME position for a player.
	UPROPERTY()
	UPlayerStartLocatorBase* PlayerStartLocator;

	UPROPERTY()
	USpatialVisualizer* Visualizer;

	/* Implemented by UChanneldNetConnection::SetSentSpawned
	// Object that are spawned in the server but don't need to send the spawn to the client connection.
	// Common case: handover pawn should not be sent to the client that owns the pawn.
	TMap<TWeakObjectPtr<UObject>, ConnectionId> ServerIgnoreSendSpawnObjects;
	*/
	
	bool bSuppressAddProviderAndSendOnServerSpawn = false;
	bool bSuppressSendOnServerDestroy = false;

	// [Client-Only] The NetId of objects that are deleted during the handover. They should not be spawned again via CheckUnspawnedObject(),
	// until the client gains interest in them again.
	TSet<uint32> SuppressedNetIdsToResolve;

	// Virtual NetConnection for sending Spawn message to channeld to broadcast in spatial channels.
	// Exporting the NetId of the spawned object requires a NetConnection, but we don't have a specific client when broadcasting.
	// So we use a virtual NetConnection that doesn't belong to any client, and clear the export map everytime to make sure the NetId is fully exported.
	UPROPERTY()
	UChanneldNetConnection* NetConnForSpawn;

	// Synchronize the NetworkGUIDs of the static and well-known objects across the spatial servers.
	void SyncNetIds();
	
	void ServerHandleSyncNetId(UChanneldConnection* ChanneldConnection, unsigned I, const google::protobuf::Message* Message);
	void ServerHandleSubToChannel(UChanneldConnection* _, Channeld::ChannelId ChId, const google::protobuf::Message* Msg);
	void ServerHandleGetHandoverContext(UChanneldConnection* _, Channeld::ChannelId ChId, const google::protobuf::Message* Msg);
	void ServerHandleHandover(UChanneldConnection* _, Channeld::ChannelId ChId, const google::protobuf::Message* Msg);
	void ClientHandleSubToChannel(UChanneldConnection* _, Channeld::ChannelId ChId, const google::protobuf::Message* Msg);
	void ClientHandleHandover(UChanneldConnection* _, Channeld::ChannelId ChId, const google::protobuf::Message* Msg);
	void ClientHandleGetUnrealObjectRef(UChanneldConnection* _, Channeld::ChannelId ChId, const google::protobuf::Message* Msg);
	
	/**
	 * @brief Create the ChanneldNetConnection for the client. The PlayerController will not be created for the connection.
	 * @param ConnId The channeld connection ID of the client
	 * @param ChId The spatial channel the client belongs to 
	 * @return 
	 */
	UChanneldNetConnection* CreateClientConnection(Channeld::ConnectionId ConnId, Channeld::ChannelId ChId);
	void InitPlayerController(UChanneldNetConnection* ClientConn, APlayerController* NewPlayerController);
	void CreatePlayerController(UChanneldNetConnection* ClientConn);
};
