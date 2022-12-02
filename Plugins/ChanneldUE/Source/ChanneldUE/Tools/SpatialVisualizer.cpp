// Fill out your copyright notice in the Description page of Project Settings.


#include "SpatialVisualizer.h"

#include "ChanneldConnection.h"
#include "ChanneldSettings.h"

USpatialVisualizer::USpatialVisualizer(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void USpatialVisualizer::Initialize(UChanneldConnection* Conn)
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	Conn->AddMessageHandler(channeldpb::SPATIAL_REGIONS_UPDATE, this, &USpatialVisualizer::HandleSpatialRegionsResult);
	Conn->AddMessageHandler(channeldpb::SUB_TO_CHANNEL, this, &USpatialVisualizer::UpdateSubBoxes);
	Conn->AddMessageHandler(channeldpb::UNSUB_FROM_CHANNEL, this, &USpatialVisualizer::UpdateSubBoxes);

	channeldpb::DebugGetSpatialRegionsMessage Msg;
	Conn->Send(GlobalChannelId, channeldpb::DEBUG_GET_SPATIAL_REGIONS, Msg);
}

void USpatialVisualizer::HandleSpatialRegionsResult(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{
	const auto Settings = GetMutableDefault<UChanneldSettings>();
	ensureMsgf(Settings->RegionBoxClass, TEXT("RegionBoxClass is not set!"));

	const auto ResultMsg = static_cast<const channeldpb::SpatialRegionsUpdateMessage*>(Msg);
	Regions.Empty();
	uint32 MaxServerIndex = 0;
	for (auto Region : ResultMsg->regions())
	{
		Regions.Add(Region);
		if (Region.serverindex() > MaxServerIndex)
		{
			MaxServerIndex = Region.serverindex();
		}
	}

	for (const auto Box : RegionBoxes)
	{
		GetWorld()->DestroyActor(Box);
	}
	RegionBoxes.Empty();

	const uint32 ServerCount = MaxServerIndex + 1;
	for (uint32 i = 0; i < ServerCount; i++)
	{
		Colors.Add(FLinearColor::MakeFromHSV8(256 * i / ServerCount, 128, 128));
	}

	for (auto Region : Regions)
	{
		// Swap the Y and Z as UE uses the Z-Up rule but channeld uses the Y-up rule.
		FVector BoundsMin = FVector(Region.min().x(), Region.min().z(), Region.min().y());
		FVector BoundsMax = FVector(Region.max().x(), Region.max().z(), Region.max().y());
		FVector Location = 0.5f * (BoundsMin + BoundsMax);
		ATintActor* Box = CastChecked<ATintActor>(GetWorld()->SpawnActor(Settings->RegionBoxClass, &Location));
		FVector BoundsSize = ClampVector(BoundsMax - BoundsMin, Settings->RegionBoxMinSize, Settings->RegionBoxMaxSize);
		FVector BoxSize = Box->GetRootComponent()->Bounds.BoxExtent * 2;
		Box->SetActorScale3D(BoundsSize / BoxSize);
		Box->SetColor(Colors[Region.serverindex()]);
		RegionBoxes.Add(Box);
	}
}

void USpatialVisualizer::UpdateSubBoxes(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{
}

