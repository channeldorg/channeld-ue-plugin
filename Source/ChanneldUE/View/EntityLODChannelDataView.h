#pragma once

#include "CoreMinimal.h"
#include "ChannelDataView.h"
#include "EntityLODChannelDataView.generated.h"

class UChanneldConnection;

USTRUCT(BlueprintType)
struct CHANNELDUE_API FLOD_Definition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float Distance = 0;

	UPROPERTY(EditAnywhere)
	uint32 FanOutInterval = 0;
};
/**
 * 
 */
UCLASS()
class CHANNELDUE_API UEntityLODChannelDataView : public UChannelDataView
{
	GENERATED_BODY()
	
public:
	UEntityLODChannelDataView(const FObjectInitializer& ObjectInitializer);

	virtual Channeld::ChannelId GetOwningChannelId(UObject* Obj) const override;
	virtual bool OnServerSpawnedObject(UObject* Obj, const FNetworkGUID NetId) override;
	virtual void SendSpawnToConn(UObject* Obj, UChanneldNetConnection* NetConn, uint32 OwningConnId) override;
	virtual void SendSpawnToClients(UObject* Obj, uint32 OwningConnId) override;
	virtual void OnNetSpawnedObject(UObject* Obj, const Channeld::ChannelId ChId, const unrealpb::SpawnObjectMessage* SpawnMsg = nullptr) override;
	virtual void OnDestroyedActor(AActor* Actor, const FNetworkGUID NetId) override;
	virtual void SendDestroyToClients(UObject* Obj, const FNetworkGUID NetId) override;

	UPROPERTY(EditAnywhere)
	FString Metadata;
	
	UPROPERTY(EditAnywhere)
	int GlobalChannelFanOutIntervalMs = 50;
	
	UPROPERTY(EditAnywhere)
	int GlobalChannelFanOutDelayMs = 2000;

	UPROPERTY(EditAnywhere)
	TArray<FLOD_Definition> LOD_Definitions;

protected:

	virtual void InitServer() override;
	virtual void InitClient() override;
	
	virtual void ServerHandleClientUnsub(Channeld::ConnectionId ClientConnId, channeldpb::ChannelType ChannelType, Channeld::ChannelId ChId) override;
	
	virtual void SendSpawnToClients_EntityChannelReady(const FNetworkGUID NetId, UObject* Obj, uint32 OwningConnId);

	void OnReplicatedActorLocation(AActor* Actor, FVector& Vector);
};
