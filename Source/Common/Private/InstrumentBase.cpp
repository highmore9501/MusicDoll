#include "InstrumentBase.h"

AInstrumentBase::AInstrumentBase()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AInstrumentBase::BeginPlay()
{
    Super::BeginPlay();
}

void AInstrumentBase::Tick(float DeltaTime)
{
    AActor::Tick(DeltaTime);
}