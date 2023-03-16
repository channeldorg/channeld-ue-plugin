#include "TRG_F005_CPP_Parent.h"

#include "Net/UnrealNetwork.h"
#include "Replication/ChanneldReplicationComponent.h"

// Sets default values
ATRG_F005_CPP_Parent::ATRG_F005_CPP_Parent()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	RepComponent = CreateDefaultSubobject<UChanneldReplicationComponent>(TEXT("RepComponent"));
}

void ATRG_F005_CPP_Parent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATRG_F005_CPP_Parent, UI32Property01);
}

// Called when the game starts or when spawned
void ATRG_F005_CPP_Parent::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ATRG_F005_CPP_Parent::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

