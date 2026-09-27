#include "World/Beach/TN_BeachElement.h"
#include "Core/TN_Log.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UObjectGlobals.h"

ATN_BeachElement::ATN_BeachElement()
{
	bReplicates = true;
	SetReplicateMovement(false);
	// Relevante en toda la playa (1200 m): si no, a 150 m (lo de serie) los clientes destruyen el elemento y lo vuelven
	// a crear, con su malla, cada vez que alguien se aleja y vuelve.
	SetNetCullDistanceSquared(FMath::Square(160000.0f));
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ATN_BeachElement::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_BeachElement, Spec);
}

ATN_BeachElement* ATN_BeachElement::SpawnElement(UWorld* World, const FTransform& Transform, const FTNBeachElementSpec& InSpec)
{
	if (!World) { return nullptr; }
	const FString Path = FString::Printf(TEXT("/Script/Tortunabo.%s"), TNBeach::ClassNameOf(InSpec.Element));
	UClass* Class = FindObject<UClass>(nullptr, *Path);
	if (!Class || !Class->IsChildOf(ATN_BeachElement::StaticClass()))
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[Playa] Sin clase %s para %s: no se crea."), *Path,
			*UEnum::GetValueAsString(InSpec.Element));
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	ATN_BeachElement* Element = World->SpawnActor<ATN_BeachElement>(Class, Transform, Params);
	if (!Element) { return nullptr; }
	Element->Spec = InSpec;
	Element->FinishSpawning(Transform);
	return Element;
}

float ATN_BeachElement::GetFootprintRadius() const
{
	return static_cast<float>(TNBeach::FootprintRadius(Spec.Element)) * FMath::Max(0.1f, Spec.SizeScale);
}

void ATN_BeachElement::BeginPlay()
{
	Super::BeginPlay();
	if (!bSpecApplied)
	{
		bSpecApplied = true;
		ApplySpec();
	}
}

void ATN_BeachElement::OnRep_Spec()
{
	bSpecApplied = true;
	ApplySpec();
}
