#include "InstrumentBase.h"

AInstrumentBase::AInstrumentBase() {
    PrimaryActorTick.bCanEverTick = true;

    RootComponent =
        CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
}

void AInstrumentBase::BeginPlay() { Super::BeginPlay(); }

void AInstrumentBase::Tick(float DeltaTime) { AActor::Tick(DeltaTime); }