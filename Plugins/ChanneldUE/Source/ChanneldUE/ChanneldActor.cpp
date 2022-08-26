// Fill out your copyright notice in the Description page of Project Settings.


#include "ChanneldActor.h"
#include "ChanneldGameInstanceSubsystem.h"
#include "ChannelDataProvider.h"
#include "View/ChannelDataView.h"
#include "Engine/ActorChannel.h"
#include "Engine/PackageMapClient.h"
#include "Misc/NetworkGuid.h"

// Sets default values
AChanneldActor::AChanneldActor(const FObjectInitializer& ObjectInitializer)
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AChanneldActor::BeginPlay()
{
	Super::BeginPlay();

	for (auto RepComp : ReplicatedComponents)
	{
		if (RepComp->IsA<USceneComponent>())
		{
			USceneComponent* SceneComp = Cast<USceneComponent>(RepComp);
			SceneComponentReplicators.Add(new FChanneldSceneComponentReplicator(SceneComp, this));
		}
	}
	
	UChannelDataView* View = GetGameInstance()->GetSubsystem<UChanneldGameInstanceSubsystem>()->GetChannelDataView();
	if (View)
	{
		View->AddProvider(OwningChannelId, this);
	}
}

void AChanneldActor::EndPlay(EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);

	//if (Reason != EEndPlayReason::LevelTransition)
	{
		UChannelDataView* View = GetGameInstance()->GetSubsystem<UChanneldGameInstanceSubsystem>()->GetChannelDataView();
		if (View)
		{
			View->RemoveProviderFromAllChannels(this, true);
		}
	}
}

uint32 AChanneldActor::GetNetGUID()
{
	auto NetConnection = this->GetNetConnection();
	if (!NetConnection)
	{
		UE_LOG(LogTemp, Warning, TEXT("Actor '%s' is not owned by any net connection"), *GetFName().ToString());
		return 0;
	}
	return NetConnection->Driver->GuidCache->GetNetGUID(this).Value;
	/*
	auto ActorChannel = NetConnection->FindActorChannelRef(this);
	if (!ActorChannel)
	{
		UE_LOG(LogTemp, Warning, TEXT("Actor '%s' doesn't have any actor channel"), *GetFName().ToString());
		return 0;
	}

	return ActorChannel->ActorNetGUID.Value;
	*/
}

// Called every frame
void AChanneldActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// TODO: update the replicators with the dynamically added/removed components

	/*
	for (auto Pair : SceneComponentReplicators)
	{
		Pair.Value->Tick(DeltaTime);
	}
	*/
}

channeldpb::ChannelType AChanneldActor::GetChannelType()
{
	return static_cast<channeldpb::ChannelType>(ChannelType);
}

google::protobuf::Message* AChanneldActor::GetChannelDataTemplate() const
{
	auto MsgObj = ProvideChannelDataTemplate();
	if (MsgObj)
	{
		return MsgObj->GetMessage();
	}
	else
	{
		UE_LOG(LogChanneld, Error, TEXT("ChanneldActor '%s' failed to provider channel data template"), *GetFName().ToString());
		return nullptr;
	}
}

ChannelId AChanneldActor::GetChannelId()
{
	return OwningChannelId;
}

void AChanneldActor::SetChannelId(ChannelId ChId)
{
	OwningChannelId = ChId;
}

bool AChanneldActor::IsRemoved()
{
	return bRemoved;
}

void AChanneldActor::SetRemoved()
{
	bRemoved = true;
}

bool AChanneldActor::UpdateChannelData(google::protobuf::Message* ChannelData)
{
	if (!HasAuthority())
	{
		return false;
	}

	bool bUpdated = false;
	for (auto Replicator : SceneComponentReplicators)
	{
		Replicator->Tick(0);
		if (Replicator->IsStateChanged())
		{
			SetSceneComponentStateToChannelData(Replicator->GetState(), ChannelData, Replicator->GetNetGUID());
			Replicator->ClearState();
			bUpdated = true;
		}
	}

	return bUpdated;
}

void AChanneldActor::OnChannelDataUpdated(google::protobuf::Message* ChannelData)
{
	for (auto Replicator : SceneComponentReplicators)
	{
		auto State = GetSceneComponentStateFromChannelData(ChannelData, Replicator->GetNetGUID());
		if (State)
		{
			if (State->removed())
			{
				auto SceneComp = Replicator->GetSceneComponent();
				if (SceneComp)
				{
					RemoveOwnedComponent(SceneComp);
				}
				continue;
			}
			Replicator->OnStateChanged(State);
		}
	}
}
