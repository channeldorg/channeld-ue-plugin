// Fill out your copyright notice in the Description page of Project Settings.


#include "SpatialVisualizer.h"

#include "ChanneldConnection.h"
#include "ChanneldSettings.h"
#include "Engine/RendererSettings.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

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

	Conn->AddMessageHandler(channeldpb::SPATIAL_REGIONS_UPDATE, this, &USpatialVisualizer::HandleSpatialRegionsUpdate);
	Conn->AddMessageHandler(channeldpb::SUB_TO_CHANNEL, this, &USpatialVisualizer::HandleSubToChannel);
	Conn->AddMessageHandler(channeldpb::UNSUB_FROM_CHANNEL, this, &USpatialVisualizer::HandleUnsubFromChannel);

	channeldpb::DebugGetSpatialRegionsMessage Msg;
	Conn->Send(Channeld::GlobalChannelId, channeldpb::DEBUG_GET_SPATIAL_REGIONS, Msg);
}

void USpatialVisualizer::HandleSpatialRegionsUpdate(UChanneldConnection* Conn, Channeld::ChannelId ChId, const google::protobuf::Message* Msg)
{
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

	const uint32 ServerCount = MaxServerIndex + 1;
	for (uint32 i = 0; i < ServerCount; i++)
	{
		RegionColors.Add(FLinearColor::MakeFromHSV8(256 * i / ServerCount, 0x60, 0x80));
	}

	for (auto Region : Regions)
	{
		// Saturation and Value should be decided by the TintActor rather than fixed in code.
		ColorsByChId.Add(Region.channelid(), FLinearColor::MakeFromHSV8(256 * Region.serverindex() / ServerCount, 0xcc, 0xff));
	}

	FTimerHandle Handle;
	// Wait a couple of seconds for the client travel to finish, otherwise the actors created by the visualizer will be removed.
	GetWorld()->GetTimerManager().SetTimer(Handle, [&, Conn]()
	{
		SpawnRegionBoxes();
		for (auto& Pair : Conn->SubscribedChannels)
		{
			SpawnSubBox(Pair.Key);
		}
	}, 1, false, 2.0f);

}

void USpatialVisualizer::SpawnRegionBoxes()
{
	const auto Settings = GetMutableDefault<UChanneldSettings>();
	ensureMsgf(Settings->RegionBoxClass, TEXT("RegionBoxClass is not set!"));

	for (const auto Pair : RegionBoxes)
	{
		GetWorld()->DestroyActor(Pair.Value);
	}
	RegionBoxes.Empty();

	// Get the default character movement component for finding the floor.
	UCharacterMovementComponent* CharMove = nullptr;
	FVector DefaultFloorLoc = FVector::ZeroVector;
	if (Settings->bRegionBoxOnFloor)
	{
		if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
		{
			if (ACharacter* Char = PlayerController->GetCharacter())
			{
				CharMove = Char->GetCharacterMovement();
				if (CharMove)
				{
					FFindFloorResult FloorResult;
					CharMove->FindFloor(CharMove->GetActorLocation(), FloorResult, true);
					if (FloorResult.bBlockingHit)
					{
						DefaultFloorLoc = FloorResult.HitResult.ImpactPoint;
					}
				}
			}
		}
	}
	
	for (auto Region : Regions)
	{
		// Swap the Y and Z as UE uses the Z-Up rule but channeld uses the Y-up rule.
		FVector BoundsMin = FVector(Region.min().x(), Region.min().z(), Region.min().y());
		FVector BoundsMax = FVector(Region.max().x(), Region.max().z(), Region.max().y());
		FVector Location = 0.5f * (BoundsMin + BoundsMax);
		
		if (Settings->bRegionBoxOnFloor && CharMove)
		{
			FFindFloorResult FloorResult;
			CharMove->FindFloor(FVector(Location.X, Location.Y, CharMove->GetActorLocation().Z), FloorResult, true);
			if (FloorResult.bBlockingHit)
			{
				Location = FloorResult.HitResult.ImpactPoint;
			}
			else if (DefaultFloorLoc != FVector::ZeroVector)
			{
				Location = FVector(Location.X, Location.Y, DefaultFloorLoc.Z);
			}
		}
		
		ATintActor* Box = CastChecked<ATintActor>(GetWorld()->SpawnActor(Settings->RegionBoxClass, &Location));
		FVector BoundsSize = ClampVector(BoundsMax - BoundsMin, Settings->RegionBoxMinSize, Settings->RegionBoxMaxSize);
		FVector BoxSize = Box->GetRootComponent()->Bounds.BoxExtent * 2;
		Box->SetActorScale3D(BoundsSize / BoxSize);
		Box->SetColor(RegionColors[Region.serverindex()]);
		RegionBoxes.Add(Region.channelid(), Box);
	}
}

void USpatialVisualizer::SpawnSubBox(Channeld::ChannelId ChId)
{
	const auto Settings = GetMutableDefault<UChanneldSettings>();
	if (!Settings->SubscriptionBoxClass)
	{
		return;
	}

	channeldpb::SpatialRegion* SubRegion = Regions.FindByPredicate([ChId](auto Value) {return Value.channelid() == ChId; });
	if (!SubRegion)
	{
		return;
	}
	auto Region = *SubRegion;

	FVector BoundsMin = FVector(Region.min().x(), Region.min().z(), Region.min().y());
	FVector BoundsMax = FVector(Region.max().x(), Region.max().z(), Region.max().y());
	FVector Location = 0.5f * (BoundsMin + BoundsMax) + Settings->SubscriptionBoxOffset;
	ATintActor* Box = CastChecked<ATintActor>(GetWorld()->SpawnActor(Settings->SubscriptionBoxClass, &Location));
	FVector BoundsSize = ClampVector(BoundsMax - BoundsMin, Settings->SubscriptionBoxMinSize, Settings->SubscriptionBoxMaxSize);
	FVector BoxSize = Box->GetRootComponent()->Bounds.BoxExtent * 2;
	Box->SetActorScale3D(BoundsSize / BoxSize);
	Box->SetColor(RegionColors[Region.serverindex()]);
	SubBoxes.Add(Region.channelid(), Box);
}

void USpatialVisualizer::HandleSubToChannel(UChanneldConnection* Conn, Channeld::ChannelId ChId, const google::protobuf::Message* Msg)
{
	// We should wait the region boxes to be spawned before spawning the subscription box.
	if (RegionBoxes.Num() == 0)
	{
		return;
	}

	// Subscription box already exists.
	if (SubBoxes.Contains(ChId))
	{
		return;
	}
	
	const auto ResultMsg = static_cast<const channeldpb::SubscribedToChannelResultMessage*>(Msg);
	if (ResultMsg->connid() != Conn->GetConnId())
	{
		return;
	}

	SpawnSubBox(ChId);
}

void USpatialVisualizer::HandleUnsubFromChannel(UChanneldConnection* Conn, Channeld::ChannelId ChId, const google::protobuf::Message* Msg)
{
	if (RegionBoxes.Num() == 0)
	{
		return;
	}
	
	const auto ResultMsg = static_cast<const channeldpb::UnsubscribedFromChannelResultMessage*>(Msg);
	if (ResultMsg->connid() != Conn->GetConnId())
	{
		return;
	}

	ATintActor* SubBox;
	if (SubBoxes.RemoveAndCopyValue(ChId, SubBox))
	{
		GetWorld()->DestroyActor(SubBox);
	}
}

void USpatialVisualizer::OnSpawnedObject(UObject* Obj, Channeld::ChannelId ChId)
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

void USpatialVisualizer::OnUpdateOwningChannel(UObject* Obj, Channeld::ChannelId NewChId)
{
	AOutlinerActor* Outliner = Outliners.FindRef(Obj);
	if (Outliner)
	{
		Outliner->SetOutlineColor(NewChId, GetColorByChannelId(NewChId));
	}
}

const FLinearColor& USpatialVisualizer::GetColorByChannelId(Channeld::ChannelId ChId)
{
	const FLinearColor* Color = ColorsByChId.Find(ChId);
	if (Color)
	{
		return *Color;
	}
	return FLinearColor::White;
}

