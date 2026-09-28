#include "World/ProcMap/TN_ProcEggNest.h"
#include "World/ProcMap/TN_ProcMapActorUtils.h"
#include "Game/TN_ProcMapGameMode.h"
#include "Player/TortugaCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

ATN_ProcEggNest::ATN_ProcEggNest()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Root);
	Trigger->InitSphereRadius(420.f);
	Trigger->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	NestBase = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NestBase"));
	NestBase->SetupAttachment(Root);
	NestBase->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NestBase->SetRelativeScale3D(FVector(2.2f, 2.2f, 0.25f));
	NestBase->SetRelativeLocation(FVector(0.f, 0.f, 10.f));
	if (Cylinder.Succeeded()) { NestBase->SetStaticMesh(Cylinder.Object); }

	// Pila de huevos: base de cinco, dos encima y uno en la cima.
	const FVector EggOffsets[8] = {
		FVector(0.f, 0.f, 45.f), FVector(55.f, 0.f, 45.f), FVector(-55.f, 0.f, 45.f), FVector(0.f, 55.f, 45.f),
		FVector(0.f, -55.f, 45.f), FVector(28.f, 28.f, 100.f), FVector(-28.f, -28.f, 100.f), FVector(0.f, 0.f, 150.f) };
	for (int32 i = 0; i < 8; ++i)
	{
		UStaticMeshComponent* Egg = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Egg%d"), i));
		Egg->SetupAttachment(Root);
		Egg->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Egg->SetRelativeLocation(EggOffsets[i]);
		Egg->SetRelativeScale3D(FVector(0.55f, 0.55f, 0.72f));
		if (Sphere.Succeeded()) { Egg->SetStaticMesh(Sphere.Object); }
		Eggs.Add(Egg);
	}
}

void ATN_ProcEggNest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_ProcEggNest, NestOrder);
	DOREPLIFETIME(ATN_ProcEggNest, PathProgress);
	DOREPLIFETIME(ATN_ProcEggNest, bActivated);
}

void ATN_ProcEggNest::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &ATN_ProcEggNest::OnTriggerOverlap);
	}
	if (NestMeshOverride)
	{
		NestBase->SetStaticMesh(NestMeshOverride);
		NestBase->SetRelativeScale3D(FVector(1.f));
		for (UStaticMeshComponent* Egg : Eggs) { Egg->SetVisibility(false); }
	}
	TNProcActors::Tint(NestBase, FLinearColor(0.35f, 0.24f, 0.12f));
	ApplyVisual();
}

FTransform ATN_ProcEggNest::GetRespawnTransform(int32 Slot) const
{
	const float Angle = (Slot % 8) * (UE_PI / 4.f);
	const FVector Offset(FMath::Cos(Angle) * 260.f, FMath::Sin(Angle) * 260.f, 120.f);
	return FTransform(GetActorRotation(), GetActorLocation() + Offset);
}

void ATN_ProcEggNest::MarkActivated()
{
	if (!HasAuthority() || bActivated) { return; }
	bActivated = true;
	OnRep_Activated();
}

void ATN_ProcEggNest::OnRep_Activated()
{
	ApplyVisual();
	if (bActivated)
	{
		if (ActivateSound) { UGameplayStatics::PlaySoundAtLocation(this, ActivateSound, GetActorLocation()); }
		OnNestActivated();
	}
}

void ATN_ProcEggNest::ApplyVisual()
{
	for (UStaticMeshComponent* Egg : Eggs)
	{
		TNProcActors::Tint(Egg, bActivated ? ActiveColor : IdleColor);
	}
}

void ATN_ProcEggNest::OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(OtherActor);
	if (!Turtle || Turtle->IsDead()) { return; }
	APlayerController* PC = Cast<APlayerController>(Turtle->GetController());
	if (!PC) { return; }
	if (ATN_ProcMapGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ATN_ProcMapGameMode>() : nullptr)
	{
		GM->NotifyEggNestReached(PC, this);
	}
}
