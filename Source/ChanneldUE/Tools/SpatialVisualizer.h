#pragma once

#include "CoreMinimal.h"
#include "TintActor.h"
#include "channeld.pb.h"
#include "ChanneldConnection.h"
#include "OutlinerActor.h"
#include "SpatialVisualizer.generated.h"

UCLASS()
class USpatialVisualizer : public UObject
{
	GENERATED_BODY()
public:
	USpatialVisualizer(const FObjectInitializer& ObjectInitializer);

	void Initialize(UChanneldConnection* Conn);
	void HandleSpatialRegionsUpdate(UChanneldConnection* Conn, Channeld::ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleSubToChannel(UChanneldConnection* Conn, Channeld::ChannelId ChId, const google::protobuf::Message* Msg);
	void HandleUnsubFromChannel(UChanneldConnection* Conn, Channeld::ChannelId ChId, const google::protobuf::Message* Msg);
	void OnSpawnedObject(UObject* Obj, Channeld::ChannelId ChId);
	void OnUpdateOwningChannel(UObject* Obj, Channeld::ChannelId NewChId);
	const FLinearColor& GetColorByChannelId(Channeld::ChannelId ChId);

private:

	void SpawnRegionBoxes();
	void SpawnSubBox(Channeld::ChannelId ChId);

	TArray<channeldpb::SpatialRegion> Regions;
	UPROPERTY()
	TMap<uint32, ATintActor*> RegionBoxes;
	UPROPERTY()
	TMap<uint32, ATintActor*> SubBoxes;
	TArray<FLinearColor> RegionColors;
	TMap<Channeld::ChannelId, FLinearColor> ColorsByChId;
	UPROPERTY()
	TMap<TWeakObjectPtr<UObject>, AOutlinerActor*> Outliners;
};
