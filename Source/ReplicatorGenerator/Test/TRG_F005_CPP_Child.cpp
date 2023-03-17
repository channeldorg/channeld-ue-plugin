#include "TRG_F005_CPP_Child.h"

#include "Net/UnrealNetwork.h"

// Sets default values
ATRG_F005_CPP_Child::ATRG_F005_CPP_Child()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

void ATRG_F005_CPP_Child::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATRG_F005_CPP_Child, UI32Property02);
}

// Called when the game starts or when spawned
void ATRG_F005_CPP_Child::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ATRG_F005_CPP_Child::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
