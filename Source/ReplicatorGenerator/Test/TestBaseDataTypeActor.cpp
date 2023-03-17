#include "TestBaseDataTypeActor.h"

#include "Net/UnrealNetwork.h"
#include "Replication/ChanneldReplicationComponent.h"

// Sets default values
ATestBaseDataTypeActor::ATestBaseDataTypeActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	UChanneldReplicationComponent* Comp = CreateDefaultSubobject<UChanneldReplicationComponent>(TEXT("test"));
}

void ATestBaseDataTypeActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATestBaseDataTypeActor, UI32Property01);
}

// Called when the game starts or when spawned
void ATestBaseDataTypeActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ATestBaseDataTypeActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

