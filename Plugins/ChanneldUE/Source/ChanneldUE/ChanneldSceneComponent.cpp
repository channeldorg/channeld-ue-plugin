// Fill out your copyright notice in the Description page of Project Settings.


#include "ChanneldSceneComponent.h"
#include "ChanneldUtils.h"
#include "ChanneldTypes.h"
//#include "ChanneldConnection.h"
//
//DEFINE_LOG_CATEGORY(LogChanneld);

// Sets default values for this component's properties
UChanneldSceneComponent::UChanneldSceneComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	TransformUpdated.AddUObject(this, &UChanneldSceneComponent::OnTransformUpdated);

	State = new unrealpb::SceneComponentState;
	RelativeLocationState = new unrealpb::FVector;
	RelativeRotationState = new unrealpb::FVector;
	RelativeScaleState = new unrealpb::FVector;
}

void UChanneldSceneComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);

	delete State;
	delete RelativeLocationState;
	delete RelativeRotationState;
	delete RelativeScaleState;
}

void UChanneldSceneComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	// Skip the Replicated properties defined in SceneComponent
	UActorComponent::GetLifetimeReplicatedProps(OutLifetimeProps);
}

// Called when the game starts
void UChanneldSceneComponent::BeginPlay()
{
	Super::BeginPlay();

	StateProvider = Cast<ISceneComponentStateProvider>(GetOwner());
	if (!StateProvider)
	{
		auto Interfaces = GetOwner()->GetComponentsByInterface(USceneComponentStateProvider::StaticClass());
		if (Interfaces.Num() > 0)
		{
			StateProvider = Cast<ISceneComponentStateProvider>(Interfaces[0]);
		}
	}

	if (StateProvider)
	{
		StateProvider->OnSceneComponentUpdated.AddUObject(this, &UChanneldSceneComponent::OnStateChanged);
	}
	else
	{
		UE_LOG(LogChanneld, Warning, TEXT("Can't find ISceneComponentStateProvider for the ChanneldSceneComponent, actor: %s"), *GetOwner()->GetName());
	}
}

void UChanneldSceneComponent::EndPlay(EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);

	if (StateProvider)
	{
		StateProvider->OnSceneComponentUpdated.RemoveAll(this);
	}
}

bool SetIfNotSame(unrealpb::FVector* VectorToSet, const FVector& VectorToCheck)
{
	bool bNotSame = false;
	if (VectorToSet->x() != VectorToCheck.X)
	{
		VectorToSet->set_x(VectorToCheck.X);
		bNotSame = true;
	}
	if (VectorToSet->y() != VectorToCheck.Y)
	{
		VectorToSet->set_y(VectorToCheck.Y);
		bNotSame = true;
	}
	if (VectorToSet->z() != VectorToCheck.Z)
	{
		VectorToSet->set_z(VectorToCheck.Z);
		bNotSame = true;
	}
	return bNotSame;
}

void UChanneldSceneComponent::OnTransformUpdated(USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	if (!StateProvider)
		return;

	if (SetIfNotSame(RelativeLocationState, GetRelativeLocation()))
	{
		bStateChanged = true;
		State->set_allocated_relativelocation(RelativeLocationState);
	}
	else
	{
		State->release_relativelocation();
	}

	if (SetIfNotSame(RelativeRotationState, GetRelativeRotation().Vector()))
	{
		bStateChanged = true;
		State->set_allocated_relativerotation(RelativeRotationState);
	}
	else
	{
		State->release_relativerotation();
	}

	if (SetIfNotSame(RelativeScaleState, GetRelativeScale3D()))
	{
		bStateChanged = true;
		State->set_allocated_relativescale(RelativeScaleState);
	}
	else
	{
		State->release_relativescale();
	}
}

void UChanneldSceneComponent::OnStateChanged(const unrealpb::SceneComponentState* NewState)
{
	State->CopyFrom(*NewState);
	bStateChanged = false;

	if (State->has_relativelocation())
	{
		SetRelativeLocation_Direct(ChanneldUtils::GetVector(State->relativelocation()));
	}

	if (State->has_relativerotation())
	{
		SetRelativeRotation_Direct(ChanneldUtils::GetRotator(State->relativerotation()));
	}

	if (State->has_relativescale())
	{
		SetRelativeScale3D_Direct(ChanneldUtils::GetVector(State->relativescale()));
	}
}

void UChanneldSceneComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!StateProvider)
		return;

	if (State->isvisible() != IsVisible())
	{
		State->set_isvisible(IsVisible());
		bStateChanged = true;
	}

	// TODO: AttachSocketName and other properties...

	if (bStateChanged)
	{
		StateProvider->UpdateSceneComponent(State);
		bStateChanged = false;
	}
}

/*
channeldpb::ChannelType UChanneldSceneComponent::GetChannelType()
{
	return static_cast<channeldpb::ChannelType>(ChannelType);
}

google::protobuf::Message* UChanneldSceneComponent::GetChannelDataTemplate() const
{
	return new unrealpb::SceneComponentState;
}

ChannelId UChanneldSceneComponent::GetChannelId()
{
	return OwningChannelId;
}

void UChanneldSceneComponent::SetChannelId(ChannelId ChId)
{
	OwningChannelId = ChId;
}

bool UChanneldSceneComponent::IsRemoved()
{
	return bRemoved;
}

void UChanneldSceneComponent::SetRemoved()
{
	bRemoved = true;
}

bool UChanneldSceneComponent::UpdateChannelData(google::protobuf::Message* ChannelData)
{
	if (bStateChanged)
	{

		bStateChanged = false;
		return true;
	}

	return false;
}

void UChanneldSceneComponent::OnChannelDataUpdated(const google::protobuf::Message* ChannelData)
{
	throw std::logic_error("The method or operation is not implemented.");
}
*/

