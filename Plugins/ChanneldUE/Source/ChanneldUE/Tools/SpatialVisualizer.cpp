// Fill out your copyright notice in the Description page of Project Settings.


#include "SpatialVisualizer.h"

#include "ChanneldConnection.h"
#include "ChanneldSettings.h"
#include "Engine/RendererSettings.h"

USpatialVisualizer::USpatialVisualizer(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void USpatialVisualizer::Initialize(UChanneldConnection* Conn)
{
	// The visualizer only runs in the client
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

		ColorsByChId.Add(Region.channelid(), Colors[Region.serverindex()]);
	}
}

void USpatialVisualizer::UpdateSubBoxes(UChanneldConnection* Conn, ChannelId ChId, const google::protobuf::Message* Msg)
{
}

void USpatialVisualizer::OnSpawnedObject(UObject* Obj, ChannelId ChId)
{
	if (Outliners.Contains(Obj))
	{
		return;
	}

	// Only outline for actors that have locations
	if (!Obj->IsA<AActor>() || Obj->IsA<AInfo>() || Obj->IsA<AController>() || Obj->GetClass()->GetFName() == FName("GameplayDebuggerCategoryReplicator"))
	{
		return;
	}

	AActor* Actor = Cast<AActor>(Obj);
	
	const auto Settings = GetMutableDefault<UChanneldSettings>();
	if (Settings->SpatialOutlinerClass)
	{
		ensureMsgf(GetMutableDefault<URendererSettings>()->CustomDepthStencil == ECustomDepthStencil::EnabledWithStencil,
			TEXT("To enable spatial outlining, CustomDepthStencil should be set to EnabledWithStencil (see Project Settings -> Render Settings)"));

		const FVector Location = Actor->GetActorLocation();
		AOutlinerActor* Outliner = Cast<AOutlinerActor>(GetWorld()->SpawnActor(Settings->SpatialOutlinerClass, &Location));
		Outliner->SetFollowTarget(Actor);
		Outliner->SetOutlineColor(ChId, GetColorByChannelId(ChId));
		Outliners.Add(Obj, Outliner);
		
		UE_LOG(LogChanneld, Log, TEXT("Created spatial outliner for %s, size: %s"), *Actor->GetName(), *Actor->GetActorRelativeScale3D().ToCompactString());
	}
}

void USpatialVisualizer::OnUpdateOwningChannel(UObject* Obj, ChannelId NewChId)
{
	AOutlinerActor* Outliner = Outliners.FindRef(Obj);
	if (Outliner)
	{
		Outliner->SetOutlineColor(NewChId, GetColorByChannelId(NewChId));
	}
}

const FLinearColor& USpatialVisualizer::GetColorByChannelId(ChannelId ChId)
{
	const FLinearColor* Color = ColorsByChId.Find(ChId);
	if (Color)
	{
		return *Color;
	}
	return FLinearColor::White;
}

