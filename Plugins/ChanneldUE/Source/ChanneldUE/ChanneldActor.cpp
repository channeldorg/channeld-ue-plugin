// Fill out your copyright notice in the Description page of Project Settings.


#include "ChanneldActor.h"
#include "ChanneldGameInstanceSubsystem.h"
#include "ChannelDataProvider.h"
#include "View/ChannelDataView.h"
#include "Engine/ActorChannel.h"

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
			SceneComponentReplicators.Add(SceneComp, new FChanneldSceneComponentReplicator(SceneComp, this));
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
	auto ActorChannel = NetConnection->FindActorChannelRef(this);
	if (!ActorChannel)
	{
		UE_LOG(LogTemp, Warning, TEXT("Actor '%s' doesn't have any actor channel"), *GetFName().ToString());
		return 0;
	}

	return ActorChannel->ActorNetGUID.Value;
}

void AChanneldActor::OnChannelDataRemoved()
{
	Destroy();
}

// Called every frame
void AChanneldActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	for (auto Pair : SceneComponentReplicators)
	{
		Pair.Value->Tick(DeltaTime);
	}
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
	/*
	if (SceneComponentState)
	{
		SetSceneComponentStateToChannelData(SceneComponentState.Get(), ChannelData);
		SceneComponentState = nullptr;
		bUpdated = true;
	}
	*/
	for (auto Pair : SceneComponentReplicators)
	{
		if (Pair.Value->IsStateChanged())
		{
			SetSceneComponentStateToChannelData(Pair.Value->GetState(), ChannelData);
			Pair.Value->ClearState();
			bUpdated = true;
		}
	}

	return bUpdated;
}

void AChanneldActor::OnChannelDataUpdated(google::protobuf::Message* ChannelData)
{
	auto State = GetSceneComponentStateFromChannelData(ChannelData);
	if (State)
	{
		if (State->removed())
		{
			OnChannelDataRemoved();
			return;
		}
		OnSceneComponentUpdated.Broadcast(State);
	}
}

void AChanneldActor::UpdateSceneComponent(unrealpb::SceneComponentState* State)
{
/*
	SceneComponentState = MakeShared<unrealpb::SceneComponentState>();
	SceneComponentState->MergeFrom(*State);
*/
}
