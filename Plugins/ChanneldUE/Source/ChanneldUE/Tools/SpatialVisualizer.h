#pragma once

#include "CoreMinimal.h"
#include "ATintActor.h"
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
	void HandleSpatialRegionsResult(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void UpdateSubBoxes(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg);
	void OnSpawnedObject(UObject* Obj, ChannelId ChId);
	void OnUpdateOwningChannel(UObject* Obj, ChannelId NewChId);
	const FLinearColor& GetColorByChannelId(ChannelId ChId);

private:

	void SpawnRegionBoxes();

	TArray<channeldpb::SpatialRegion> Regions;
	UPROPERTY()
	TArray<ATintActor*> RegionBoxes;
	UPROPERTY()
	TArray<AActor*> SubBoxes;
	TArray<FLinearColor> RegionColors;
	TMap<ChannelId, FLinearColor> ColorsByChId;
	UPROPERTY()
	TMap<TWeakObjectPtr<UObject>, AOutlinerActor*> Outliners;
};
