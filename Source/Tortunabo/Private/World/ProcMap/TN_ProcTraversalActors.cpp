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
#include "Materials/MaterialInterface.h"
#include "TN_ProcMapMeshKit.h"
#include "TN_ProcMapRuntimeMesh.h"
#include "TN_ProcMapAmbientFX.h"

namespace
{
	using namespace TNProcMesh;

	/**
	 * Montículo de sínter del géiser (local, base en el suelo): terrazas concéntricas de depósito mineral
	 * (anaranjado de bacterias fuera, crema y blanco dentro), poza turquesa arriba, boca oscura con reborde
	 * y piedras alrededor. Bajo (34 cm): se sube sin saltar.
	 */
	void TNGeyserMound(FTNProcMeshBuffers& M, uint32 Seed)
	{
		const double Radii[4] = { 300.0, 238.0, 172.0, 110.0 };
		const double Tops[4] = { 9.0, 17.0, 25.0, 33.0 };
		const FLinearColor Cols[4] = { FLinearColor(0.84f, 0.5f, 0.2f), FLinearColor(0.9f, 0.76f, 0.48f), FLinearColor(0.92f, 0.9f, 0.83f), FLinearColor(0.83f, 0.83f, 0.8f) };
		double Z0 = -12.0;
		for (int32 k = 0; k < 4; ++k)
		{
			TNProcAddLathe(M, FVector::ZeroVector, { Z0, Tops[k] }, { Radii[k] * 1.04, Radii[k] }, 0.05, Seed + static_cast<uint32>(k) * 13u, Cols[k], 14, 0.0);
			Z0 = Tops[k] - 4.0;
		}
		TNProcAddLathe(M, FVector::ZeroVector, { 33.0, 34.5 }, { 96.0, 92.0 }, 0.03, Seed + 71u, FLinearColor(0.14f, 0.6f, 0.68f), 14, 0.0);
		TNProcAddCylinder(M, FVector(0.0, 0.0, 30.0), FVector(0.0, 0.0, 41.0), 44.0, 38.0, 10, FLinearColor(0.78f, 0.76f, 0.7f), false);
		TNProcAddCylinder(M, FVector(0.0, 0.0, 34.0), FVector(0.0, 0.0, 36.0), 36.0, 36.0, 10, FLinearColor(0.06f, 0.07f, 0.08f), true);
		for (int32 r = 0; r < 7; ++r)
		{
			const double A = TNProcMap::TwoPi * r / 7 + 0.4 * TNProcHashNoise(r, 3, Seed);
			const double D = 310.0 + 40.0 * TNProcHashNoise(r, 5, Seed);
			TNProcAddBoulder(M, FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, -6.0), 22.0 + 14.0 * (0.5 + 0.5 * TNProcHashNoise(r, 7, Seed)), 30.0, Seed + static_cast<uint32>(r), FLinearColor(0.45f, 0.42f, 0.4f));
		}
	}

	/** Chorro del géiser: columna de agua abultada de 100 cm (se estira con el pulso), blanca arriba. */
	void TNGeyserJet(FTNProcMeshBuffers& M)
	{
		const double Z[] = { 0.0, 10.0, 24.0, 38.0, 52.0, 66.0, 80.0, 92.0, 100.0 };
		const double R[] = { 50.0, 60.0, 46.0, 57.0, 43.0, 50.0, 36.0, 24.0, 6.0 };
		for (int32 k = 0; k + 1 < 9; ++k)
		{
			const float T = static_cast<float>(k) / 8.f;
			const FLinearColor Col = TNProcLerpColor(FLinearColor(0.55f, 0.82f, 0.95f), FLinearColor(0.95f, 0.98f, 1.0f), T);
			TNProcAddCylinder(M, FVector(0.0, 0.0, Z[k]), FVector(0.0, 0.0, Z[k + 1]), R[k], R[k + 1], 9, Col, k == 7);
		}
	}
}

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
	PulseTime = FMath::FRand() * 3.f;

	// Low-poly propio: montículo de sínter (con colisión: se sube como un escalón) y chorro que pulsa.
	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ProcMap/Materials/M_ProcFoliage.M_ProcFoliage"));
	const uint32 Seed = static_cast<uint32>(GetTypeHash(GetActorLocation()));
	FTNProcMeshBuffers Mound, Jet;
	TNGeyserMound(Mound, Seed);
	TNGeyserJet(Jet);
	UStaticMesh* MoundMesh = Mat ? TNProcRuntimeMesh::MakeStaticMesh(this, Mound, Mat, true, 0.f, 1.f, 0.f) : nullptr;
	UStaticMesh* JetMesh = Mat ? TNProcRuntimeMesh::MakeStaticMesh(this, Jet, Mat, false, 0.f, 1.f, 0.f) : nullptr;
	if (MoundMesh && JetMesh)
	{
		BaseMesh->SetStaticMesh(MoundMesh);
		BaseMesh->SetRelativeScale3D(FVector::OneVector);
		BaseMesh->SetRelativeLocation(FVector::ZeroVector);
		BaseMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		BaseMesh->SetCollisionProfileName(TEXT("BlockAll"));
		ColumnMesh->SetStaticMesh(JetMesh);
		ColumnMesh->SetRelativeLocation(FVector(0.f, 0.f, 30.f));
	}
	else
	{
		TNProcActors::Tint(BaseMesh, FLinearColor(0.18f, 0.16f, 0.14f));
		TNProcActors::Tint(ColumnMesh, FLinearColor(0.55f, 0.8f, 1.f));
	}

	// Efectos: chorro de gotas que sube y cae alrededor, vapor que asciende y salpicadura en la boca.
	const FVector Base = GetActorLocation();
	TNAmbientFX::FEmitterDesc Spray;
	Spray.Shape = TNAmbientFX::EShape::Drop;
	Spray.Color = FLinearColor(0.72f, 0.9f, 1.f);
	Spray.MaxParticles = 90;
	Spray.Rate = 55.f;
	Spray.SpawnRadius = 45.f;
	Spray.Speed = 1050.f;
	Spray.SpeedJitter = 0.25f;
	Spray.Spread = 0.16f;
	Spray.Drag = 0.05f;
	Spray.LifeMin = 1.4f;
	Spray.LifeMax = 2.2f;
	Spray.SizeStart = 14.f;
	Spray.SizeEnd = 9.f;
	TNAmbientFX::AddEmitter(this, Spray, Base + FVector(0.f, 0.f, 45.f));
	TNAmbientFX::FEmitterDesc Steam;
	Steam.Shape = TNAmbientFX::EShape::Puff;
	Steam.bSoft = true;
	Steam.Color = FLinearColor(0.95f, 0.97f, 1.f);
	Steam.Alpha = 0.35f;
	Steam.MaxParticles = 26;
	Steam.Rate = 5.f;
	Steam.SpawnRadius = 90.f;
	Steam.Speed = 90.f;
	Steam.Spread = 0.5f;
	Steam.Gravity = 0.f;
	Steam.Buoyancy = 55.f;
	Steam.Drag = 0.4f;
	Steam.LifeMin = 3.5f;
	Steam.LifeMax = 5.5f;
	Steam.SizeStart = 90.f;
	Steam.SizeEnd = 320.f;
	Steam.WakeDistance = 20000.f;
	TNAmbientFX::AddEmitter(this, Steam, Base + FVector(0.f, 0.f, 60.f));
	TNAmbientFX::FEmitterDesc Splash;
	Splash.Shape = TNAmbientFX::EShape::Drop;
	Splash.Color = FLinearColor(0.85f, 0.95f, 1.f);
	Splash.MaxParticles = 40;
	Splash.Rate = 26.f;
	Splash.SpawnRadius = 30.f;
	Splash.Speed = 320.f;
	Splash.Spread = 0.95f;
	Splash.LifeMin = 0.5f;
	Splash.LifeMax = 0.9f;
	Splash.SizeStart = 9.f;
	Splash.SizeEnd = 6.f;
	TNAmbientFX::AddEmitter(this, Splash, Base + FVector(0.f, 0.f, 40.f));
}

void ATN_ProcGeyser::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Chorro que respira a borbotones (solo visual, local): la columna se estira y las gotas salen al ritmo.
	PulseTime += DeltaTime;
	const float Pulse = FMath::Clamp(0.5f + 0.35f * FMath::Sin(PulseTime * 3.1f) + 0.15f * FMath::Sin(PulseTime * 7.3f), 0.f, 1.f);
	ColumnMesh->SetRelativeScale3D(FVector(0.9f + 0.35f * Pulse, 0.9f + 0.35f * Pulse, 3.f + 4.f * Pulse));
	for (int32 k = 0; k < 3; k += 2)
	{
		if (TNAmbientFX::FEmitter* E = TNAmbientFX::GetEmitter(this, k)) { E->RateScale = 0.35f + 0.65f * Pulse; }
	}
	TNAmbientFX::TickOwner(this, DeltaTime);
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

	// Pie de la cascada: salpicaduras hacia delante, espuma que se abre sobre la pocita y bruma.
	TNAmbientFX::RemoveOwner(this);
	if (Points.Num() >= 2)
	{
		const FVector Foot = Points.Last();
		const FVector Flow = (Points.Last() - Points[Points.Num() - 2]).GetSafeNormal2D();
		TNAmbientFX::FEmitterDesc Splash;
		Splash.Shape = TNAmbientFX::EShape::Drop;
		Splash.Color = FLinearColor(0.82f, 0.94f, 1.f);
		Splash.MaxParticles = 60;
		Splash.Rate = 34.f;
		Splash.SpawnRadius = Width * 0.3f;
		Splash.Direction = (FVector::UpVector * 1.3f + Flow).GetSafeNormal();
		Splash.Speed = 420.f;
		Splash.Spread = 0.6f;
		Splash.LifeMin = 0.7f;
		Splash.LifeMax = 1.3f;
		Splash.SizeStart = 11.f;
		Splash.SizeEnd = 7.f;
		TNAmbientFX::AddEmitter(this, Splash, Foot + FVector(0.f, 0.f, 30.f));
		TNAmbientFX::FEmitterDesc Foam;
		Foam.Shape = TNAmbientFX::EShape::Puff;
		Foam.bSoft = true;
		Foam.Color = FLinearColor(1.f, 1.f, 1.f);
		Foam.Alpha = 0.6f;
		Foam.MaxParticles = 24;
		Foam.Rate = 7.f;
		Foam.SpawnRadius = Width * 0.25f;
		Foam.Direction = Flow;
		Foam.Speed = 70.f;
		Foam.Spread = 1.f;
		Foam.Gravity = 0.f;
		Foam.Drag = 0.5f;
		Foam.LifeMin = 2.f;
		Foam.LifeMax = 3.f;
		Foam.SizeStart = 50.f;
		Foam.SizeEnd = 150.f;
		TNAmbientFX::AddEmitter(this, Foam, Foot + FVector(0.f, 0.f, 12.f));
		TNAmbientFX::FEmitterDesc Mist;
		Mist.Shape = TNAmbientFX::EShape::Puff;
		Mist.bSoft = true;
		Mist.Color = FLinearColor(0.92f, 0.96f, 1.f);
		Mist.Alpha = 0.25f;
		Mist.MaxParticles = 18;
		Mist.Rate = 3.5f;
		Mist.SpawnRadius = Width * 0.4f;
		Mist.Speed = 60.f;
		Mist.Spread = 0.6f;
		Mist.Gravity = 0.f;
		Mist.Buoyancy = 30.f;
		Mist.Drag = 0.4f;
		Mist.LifeMin = 3.f;
		Mist.LifeMax = 4.5f;
		Mist.SizeStart = 110.f;
		Mist.SizeEnd = 320.f;
		Mist.WakeDistance = 20000.f;
		TNAmbientFX::AddEmitter(this, Mist, Foot + FVector(0.f, 0.f, 60.f));
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
	TNAmbientFX::TickOwner(this, DeltaTime);

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
