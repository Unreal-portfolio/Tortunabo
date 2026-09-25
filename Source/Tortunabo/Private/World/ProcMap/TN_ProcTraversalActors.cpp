#include "World/ProcMap/TN_ProcTraversalActors.h"
#include "World/ProcMap/TN_ProcMapActorUtils.h"
#include "Player/TortugaCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

// ─────────────────────────────────────────────────────────────────────────────
// Géiser
// ─────────────────────────────────────────────────────────────────────────────

ATN_ProcGeyser::ATN_ProcGeyser()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Trigger = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Root);
	Trigger->InitCapsuleSize(190.f, 160.f);
	Trigger->SetRelativeLocation(FVector(0.f, 0.f, 140.f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(Root);
	BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BaseMesh->SetRelativeScale3D(FVector(3.2f, 3.2f, 0.25f));
	BaseMesh->SetRelativeLocation(FVector(0.f, 0.f, 5.f));

	ColumnMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ColumnMesh"));
	ColumnMesh->SetupAttachment(Root);
	ColumnMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ColumnMesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 3.f));
	ColumnMesh->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	ColumnMesh->SetCastShadow(false);

	if (Cylinder.Succeeded())
	{
		BaseMesh->SetStaticMesh(Cylinder.Object);
		ColumnMesh->SetStaticMesh(Cylinder.Object);
	}

	SprayVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SprayVFX"));
	SprayVFX->SetupAttachment(Root);
	SprayVFX->bAutoActivate = true;
}

void ATN_ProcGeyser::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ATN_ProcGeyser::OnTriggerOverlap);
	TNProcActors::Tint(BaseMesh, FLinearColor(0.18f, 0.16f, 0.14f));
	TNProcActors::Tint(ColumnMesh, FLinearColor(0.55f, 0.8f, 1.f));
	PulseTime = FMath::FRand() * 3.f;
}

void ATN_ProcGeyser::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Columna de agua que respira: solo visual, local.
	PulseTime += DeltaTime;
	const float Pulse = 0.5f + 0.5f * FMath::Sin(PulseTime * 4.f);
	ColumnMesh->SetRelativeScale3D(FVector(1.0f + 0.3f * Pulse, 1.0f + 0.3f * Pulse, 2.2f + 2.4f * Pulse));
	ColumnMesh->SetRelativeLocation(FVector(0.f, 0.f, 50.f * (2.2f + 2.4f * Pulse)));
}

void ATN_ProcGeyser::OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		if (TNProcActors::SimulatesMovement(Character))
		{
			Launch(Character);
		}
	}
}

void ATN_ProcGeyser::Launch(ACharacter* Character)
{
	UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	if (!Move || !GetWorld())
	{
		return;
	}

	// Antirrebote: un overlap por pisada.
	const double Now = GetWorld()->GetTimeSeconds();
	if (const double* Last = LastLaunchTime.Find(Character))
	{
		if (Now - *Last < 0.6) { return; }
	}
	LastLaunchTime.Add(Character, Now);

	// Parábola que pasa por un ápice por encima del punto más alto y cae en Target.
	const float Gravity = FMath::Max(1.f, -Move->GetGravityZ());
	const FVector Start = Character->GetActorLocation();
	const float HalfHeight = Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	const FVector Land = Target + FVector(0.f, 0.f, HalfHeight + 30.f);
	const float Apex = FMath::Max(Start.Z, Land.Z) + ApexExtra;
	const float Vz = FMath::Sqrt(2.f * Gravity * (Apex - Start.Z));
	const float TUp = Vz / Gravity;
	const float TDown = FMath::Sqrt(FMath::Max(0.f, 2.f * (Apex - Land.Z) / Gravity));
	const FVector Flat(Land.X - Start.X, Land.Y - Start.Y, 0.f);
	const FVector Velocity = Flat / FMath::Max(0.1f, TUp + TDown) + FVector(0.f, 0.f, Vz);

	if (ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(Character))
	{
		Turtle->SetFallImmuneUntilLanded();
	}
	Character->LaunchCharacter(Velocity, true, true);

	if (LaunchSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, LaunchSound, GetActorLocation());
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Tobogán
// ─────────────────────────────────────────────────────────────────────────────

ATN_ProcSlideZone::ATN_ProcSlideZone()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ATN_ProcSlideZone::InitFromPoints(const TArray<FVector>& Points, float Width)
{
	for (UBoxComponent* Seg : Segments)
	{
		if (Seg) { Seg->DestroyComponent(); }
	}
	Segments.Reset();
	SegmentDirs.Reset();

	for (int32 i = 0; i + 1 < Points.Num(); ++i)
	{
		const FVector A = Points[i];
		const FVector B = Points[i + 1];
		const FVector Dir = (B - A).GetSafeNormal();
		if (Dir.IsNearlyZero()) { continue; }

		UBoxComponent* Seg = NewObject<UBoxComponent>(this);
		Seg->SetupAttachment(Root);
		Seg->SetBoxExtent(FVector((B - A).Size() * 0.5f + 60.f, Width * 0.5f + 100.f, 220.f));
		Seg->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Seg->SetCollisionResponseToAllChannels(ECR_Ignore);
		Seg->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Seg->SetGenerateOverlapEvents(true);
		Seg->RegisterComponent();
		Seg->SetWorldLocationAndRotation((A + B) * 0.5f + FVector(0.f, 0.f, 120.f), Dir.Rotation());
		Seg->OnComponentBeginOverlap.AddDynamic(this, &ATN_ProcSlideZone::OnSegmentOverlap);
		Segments.Add(Seg);
		SegmentDirs.Add(Dir);
	}
}

void ATN_ProcSlideZone::OnSegmentOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(OtherActor))
	{
		if (TNProcActors::SimulatesMovement(Turtle))
		{
			Turtle->SetFallImmuneUntilLanded();
		}
	}
}

void ATN_ProcSlideZone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	for (int32 i = 0; i < Segments.Num(); ++i)
	{
		UBoxComponent* Seg = Segments[i];
		if (!Seg) { continue; }
		TArray<AActor*> Overlapping;
		Seg->GetOverlappingActors(Overlapping, ACharacter::StaticClass());
		for (AActor* Actor : Overlapping)
		{
			ACharacter* Character = Cast<ACharacter>(Actor);
			if (!Character || !TNProcActors::SimulatesMovement(Character)) { continue; }
			if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
			{
				// Empuje ladera abajo: la pendiente ya no es caminable, esto le da ritmo de tobogán.
				Move->AddImpulse(SegmentDirs[i] * BoostAcceleration * DeltaTime, true);
			}
			if (ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(Character))
			{
				Turtle->SetFallImmuneUntilLanded();
			}
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Volumen de muerte
// ─────────────────────────────────────────────────────────────────────────────

ATN_ProcKillVolume::ATN_ProcKillVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(200.f));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Box->SetGenerateOverlapEvents(true);
	Box->SetHiddenInGame(true);
}

void ATN_ProcKillVolume::BeginPlay()
{
	Super::BeginPlay();
	Box->OnComponentBeginOverlap.AddDynamic(this, &ATN_ProcKillVolume::OnBoxOverlap);
}

void ATN_ProcKillVolume::SetExtent(const FVector& Extent)
{
	Box->SetBoxExtent(Extent);
}

void ATN_ProcKillVolume::OnBoxOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!TNProcActors::IsServerWorld(this))
	{
		return;
	}
	if (ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(OtherActor))
	{
		Turtle->RequestKill(this);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Meta
// ─────────────────────────────────────────────────────────────────────────────

void ATN_ProcFinishVolume::SetExtent(const FVector& Extent)
{
	if (TriggerBox)
	{
		TriggerBox->SetBoxExtent(Extent);
	}
}
