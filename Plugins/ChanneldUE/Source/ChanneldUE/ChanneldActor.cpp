// Fill out your copyright notice in the Description page of Project Settings.


#include "ChanneldActor.h"
#include "ChanneldGameInstanceSubsystem.h"
#include "ChannelDataProvider.h"
#include "View/ChannelDataView.h"

// Sets default values
AChanneldActor::AChanneldActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AChanneldActor::BeginPlay()
{
	Super::BeginPlay();
	
	//this->GetClass()->ImplementsInterface(IChannelDataProvider::StaticClass())
	//if (auto Provider = Cast<IChannelDataProvider>(this))
	{
		UChannelDataView* View = GetGameInstance()->GetSubsystem<UChanneldGameInstanceSubsystem>()->GetChannelDataView();
		if (View)
		{
			View->AddProvider(OwningChannelId, this);
		}
	}
}

// Called every frame
void AChanneldActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

channeldpb::ChannelType AChanneldActor::GetChannelType()
{
	return static_cast<channeldpb::ChannelType>(ChannelType);
}

google::protobuf::Message* AChanneldActor::GetChannelDataTemplate() const
{
	throw std::logic_error("The method or operation is not implemented.");
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
	throw std::logic_error("The method or operation is not implemented.");
}

void AChanneldActor::OnChannelDataUpdated(const google::protobuf::Message* ChannelData)
{
	throw std::logic_error("The method or operation is not implemented.");
}

