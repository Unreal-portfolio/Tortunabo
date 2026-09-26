#include "World/ProcMap/TN_ProcWaterActors.h"
#include "World/ProcMap/TN_ProcMapActorUtils.h"
#include "Player/TortugaCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/BrushComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"

namespace
{
	bool TNProcIsSwimming(const ACharacter* Character)
	{
		const UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
		return Move && Move->IsSwimming();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Volumen de agua
// ─────────────────────────────────────────────────────────────────────────────

ATN_ProcWaterVolume::ATN_ProcWaterVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWaterVolume = true;
	// Dentro del agua cuando el centro de la cápsula está en una caja (ver
	// IsOverlapInVolume): la tortuga flota con medio cuerpo fuera del agua.
	bPhysicsOnContact = false;
	FluidFriction = 0.35f;
	Priority = 10;
	bReplicates = false;

	// El brush queda vacío: el agua la definen las cajas añadidas en runtime.
	if (UBrushComponent* BrushComp = GetBrushComponent())
	{
		BrushComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

bool ATN_ProcWaterVolume::IsOverlapInVolume(const USceneComponent& TestComponent) const
{
	// El APhysicsVolume de base mide contra el brush, que aquí está vacío.
	const FVector Point = TestComponent.GetComponentLocation();
	for (const UBoxComponent* WaterBox : Boxes)
	{
		if (!WaterBox)
		{
			continue;
		}
		const FVector Local = WaterBox->GetComponentTransform().InverseTransformPositionNoScale(Point);
		const FVector Extent = WaterBox->GetUnscaledBoxExtent();
		if (FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z)
		{
			return true;
		}
	}
	return false;
}

void ATN_ProcWaterVolume::AddWaterBox(const FVector& Center, const FVector& Extent)
{
	UBoxComponent* Box = NewObject<UBoxComponent>(this);
	Box->SetupAttachment(GetRootComponent());
	Box->SetBoxExtent(Extent);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionObjectType(ECC_WorldDynamic);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Box->SetGenerateOverlapEvents(true);
	Box->SetCanEverAffectNavigation(false);
	Box->CanCharacterStepUpOn = ECB_No;
	Box->RegisterComponent();
	Box->SetWorldLocation(Center);
	Boxes.Add(Box);
}

// ─────────────────────────────────────────────────────────────────────────────
// Corriente
// ─────────────────────────────────────────────────────────────────────────────

ATN_ProcWaterCurrent::ATN_ProcWaterCurrent()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(800.f, 400.f, 300.f));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Box->SetGenerateOverlapEvents(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	for (int32 i = 0; i < 3; ++i)
	{
		UStaticMeshComponent* Marker = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Marker%d"), i));
		Marker->SetupAttachment(Box);
		Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Marker->SetCastShadow(false);
		Marker->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.5f));
		if (Cone.Succeeded()) { Marker->SetStaticMesh(Cone.Object); }
		Markers.Add(Marker);
	}
}

void ATN_ProcWaterCurrent::Setup(const FVector& Extent, const FVector& InDirection, float InStrength)
{
	Box->SetBoxExtent(Extent);
	Direction = InDirection.GetSafeNormal2D();
	Strength = InStrength;
	SetActorRotation(Direction.Rotation());
	Direction = FVector(1.f, 0.f, 0.f);
	for (UStaticMeshComponent* Marker : Markers)
	{
		TNProcActors::Tint(Marker, FLinearColor(0.7f, 0.9f, 1.f));
	}
}

void ATN_ProcWaterCurrent::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const FVector WorldDir = GetActorRotation().RotateVector(Direction);

	// Marcas: conos tumbados que avanzan con la corriente sobre la superficie.
	MarkerPhase = FMath::Fmod(MarkerPhase + DeltaTime * 0.25f, 1.f);
	const FVector Ext = Box->GetUnscaledBoxExtent();
	for (int32 i = 0; i < Markers.Num(); ++i)
	{
		const float T = FMath::Fmod(MarkerPhase + i / 3.f, 1.f);
		Markers[i]->SetRelativeLocationAndRotation(FVector(-Ext.X + 2.f * Ext.X * T, (i - 1) * Ext.Y * 0.5f, Ext.Z - 10.f), FRotator(-90.f, 0.f, 0.f));
	}

	TArray<AActor*> Overlapping;
	Box->GetOverlappingActors(Overlapping, ACharacter::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		ACharacter* Character = Cast<ACharacter>(Actor);
		if (!TNProcIsSwimming(Character) || !TNProcActors::SimulatesMovement(Character)) { continue; }
		Character->GetCharacterMovement()->AddImpulse(WorldDir * Strength * DeltaTime, true);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Remolino
// ─────────────────────────────────────────────────────────────────────────────

ATN_ProcWhirlpool::ATN_ProcWhirlpool()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

	Area = CreateDefaultSubobject<USphereComponent>(TEXT("Area"));
	SetRootComponent(Area);
	Area->InitSphereRadius(1400.f);
	Area->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Area->SetCollisionResponseToAllChannels(ECR_Ignore);
	Area->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Area->SetGenerateOverlapEvents(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc"));
	Disc->SetupAttachment(Area);
	Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Disc->SetCastShadow(false);
	if (Cylinder.Succeeded()) { Disc->SetStaticMesh(Cylinder.Object); }
}

void ATN_ProcWhirlpool::BeginPlay()
{
	Super::BeginPlay();
	Area->SetSphereRadius(Radius);
	Disc->SetRelativeScale3D(FVector(Radius / 50.f, Radius / 50.f, 0.02f));
	Disc->SetRelativeLocation(FVector(0.f, 0.f, 3.f));
	TNProcActors::Tint(Disc, FLinearColor(0.02f, 0.12f, 0.25f));
}

void ATN_ProcWhirlpool::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Disc->AddLocalRotation(FRotator(0.f, 90.f * DeltaTime, 0.f));

	const FVector Center = GetActorLocation();
	const bool bServer = TNProcActors::IsServerWorld(this);
	TArray<AActor*> Overlapping;
	Area->GetOverlappingActors(Overlapping, ATortugaCharacter::StaticClass());
	TSet<TWeakObjectPtr<ATortugaCharacter>> Present;

	for (AActor* Actor : Overlapping)
	{
		ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(Actor);
		if (!TNProcIsSwimming(Turtle)) { continue; }

		const FVector ToCenter = (Center - Turtle->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
		const float Dist = ToCenter.Size();
		if (TNProcActors::SimulatesMovement(Turtle) && Dist > 1.f)
		{
			const FVector In = ToCenter / Dist;
			const FVector Tangent(-In.Y, In.X, 0.f);
			const float Closeness = 1.f - FMath::Clamp(Dist / Radius, 0.f, 1.f);
			Turtle->GetCharacterMovement()->AddImpulse((In * PullAcceleration * (0.4f + Closeness) + Tangent * SwirlAcceleration) * DeltaTime, true);
		}

		// El ojo del remolino ahoga: solo el servidor decide la muerte.
		if (bServer && Dist < Radius * 0.3f)
		{
			Present.Add(Turtle);
			float& T = EyeTime.FindOrAdd(Turtle);
			T += DeltaTime;
			if (T >= SecondsInEyeToDie)
			{
				T = 0.f;
				Turtle->RequestKill(this);
			}
		}
	}

	if (bServer)
	{
		for (auto It = EyeTime.CreateIterator(); It; ++It)
		{
			if (!Present.Contains(It.Key())) { It.RemoveCurrent(); }
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Depredador
// ─────────────────────────────────────────────────────────────────────────────

ATN_ProcWaterPredator::ATN_ProcWaterPredator()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(20.f);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Fin = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Fin"));
	Fin->SetupAttachment(Root);
	Fin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Fin->SetRelativeScale3D(FVector(0.9f, 0.25f, 0.9f));
	Fin->SetRelativeLocation(FVector(0.f, 0.f, 45.f));
	if (Cone.Succeeded()) { Fin->SetStaticMesh(Cone.Object); }

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetRelativeScale3D(FVector(3.2f, 1.1f, 0.9f));
	Body->SetRelativeLocation(FVector(0.f, 0.f, -40.f));
	if (Sphere.Succeeded()) { Body->SetStaticMesh(Sphere.Object); }
}

void ATN_ProcWaterPredator::BeginPlay()
{
	Super::BeginPlay();
	Home = GetActorLocation();
	PatrolAngle = FMath::FRand() * 6.28f;
	TNProcActors::Tint(Fin, FLinearColor(0.25f, 0.28f, 0.32f));
	TNProcActors::Tint(Body, FLinearColor(0.2f, 0.22f, 0.26f));
}

ATortugaCharacter* ATN_ProcWaterPredator::FindSwimmerNear(const FVector& Where, float Range) const
{
	ATortugaCharacter* Best = nullptr;
	float BestDist = Range;
	for (TActorIterator<ATortugaCharacter> It(GetWorld()); It; ++It)
	{
		ATortugaCharacter* Turtle = *It;
		if (!Turtle || Turtle->IsDead() || !TNProcIsSwimming(Turtle)) { continue; }
		const float D = FVector::Dist2D(Where, Turtle->GetActorLocation());
		if (D < BestDist) { BestDist = D; Best = Turtle; }
	}
	return Best;
}

void ATN_ProcWaterPredator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!HasAuthority()) { return; }

	BiteCooldown = FMath::Max(0.f, BiteCooldown - DeltaTime);
	FVector Pos = GetActorLocation();

	ATortugaCharacter* Prey = Target.Get();
	if (!Prey || !TNProcIsSwimming(Prey) || Prey->IsDead() || FVector::Dist2D(Prey->GetActorLocation(), Home) > LeashRadius)
	{
		Prey = FindSwimmerNear(Pos, DetectRadius);
		if (Prey && FVector::Dist2D(Prey->GetActorLocation(), Home) > LeashRadius) { Prey = nullptr; }
		Target = Prey;
	}

	FVector Goal;
	float Speed;
	if (Prey)
	{
		Goal = Prey->GetActorLocation();
		Speed = ChaseSpeed;
	}
	else
	{
		PatrolAngle += DeltaTime * PatrolSpeed / FMath::Max(100.f, PatrolRadius);
		Goal = Home + FVector(FMath::Cos(PatrolAngle), FMath::Sin(PatrolAngle), 0.f) * PatrolRadius;
		Speed = PatrolSpeed;
	}

	FVector Delta = (Goal - Pos) * FVector(1.f, 1.f, 0.f);
	const float Dist = Delta.Size();
	if (Dist > 1.f)
	{
		const FVector Step = Delta / Dist * FMath::Min(Dist, Speed * DeltaTime);
		Pos += Step;
		Pos.Z = Home.Z;
		SetActorLocationAndRotation(Pos, Step.Rotation());
	}

	if (Prey && BiteCooldown <= 0.f && FVector::Dist(Pos, Prey->GetActorLocation()) < BiteRadius + 90.f)
	{
		BiteCooldown = 2.f;
		if (BiteSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, BiteSound, Pos);
		}
		Prey->RequestKill(this);
		Target = nullptr;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Criatura rebotadora
// ─────────────────────────────────────────────────────────────────────────────

ATN_ProcWaterBouncer::ATN_ProcWaterBouncer()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Body->SetRelativeScale3D(FVector(2.4f, 2.4f, 1.2f));
	if (Sphere.Succeeded()) { Body->SetStaticMesh(Sphere.Object); }

	Contact = CreateDefaultSubobject<USphereComponent>(TEXT("Contact"));
	Contact->SetupAttachment(Root);
	Contact->InitSphereRadius(170.f);
	Contact->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Contact->SetCollisionResponseToAllChannels(ECR_Ignore);
	Contact->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Contact->SetGenerateOverlapEvents(true);
}

void ATN_ProcWaterBouncer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_ProcWaterBouncer, VariantMesh);
	DOREPLIFETIME(ATN_ProcWaterBouncer, VariantColor);
}

void ATN_ProcWaterBouncer::BeginPlay()
{
	Super::BeginPlay();
	Contact->OnComponentBeginOverlap.AddDynamic(this, &ATN_ProcWaterBouncer::OnContact);
	BobTime = FMath::FRand() * 6.f;
	OnRep_Variant();
}

void ATN_ProcWaterBouncer::SetVariant(UStaticMesh* Mesh, const FLinearColor& Color)
{
	VariantMesh = Mesh;
	VariantColor = Color;
	OnRep_Variant();
}

void ATN_ProcWaterBouncer::OnRep_Variant()
{
	if (VariantMesh)
	{
		Body->SetStaticMesh(VariantMesh);
	}
	TNProcActors::Tint(Body, VariantColor);
}

void ATN_ProcWaterBouncer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Balanceo y aplastamiento: puramente visual y local.
	BobTime += DeltaTime;
	SquishAlpha = FMath::Max(0.f, SquishAlpha - DeltaTime * 3.f);
	const float Bob = 12.f * FMath::Sin(BobTime * 1.7f);
	const float Squish = 1.f - 0.35f * SquishAlpha;
	Body->SetRelativeLocation(FVector(0.f, 0.f, Bob));
	Body->SetRelativeScale3D(FVector(2.4f * (2.f - Squish), 2.4f * (2.f - Squish), 1.2f * Squish));
}

void ATN_ProcWaterBouncer::OnContact(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) { return; }
	ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(OtherActor);
	if (!Turtle || Turtle->IsDead() || !GetWorld()) { return; }

	const double Now = GetWorld()->GetTimeSeconds();
	if (const double* Last = LastContact.Find(Turtle))
	{
		if (Now - *Last < 0.5) { return; }
	}
	LastContact.Add(Turtle, Now);

	const FVector Rel = Turtle->GetActorLocation() - GetActorLocation();
	if (Rel.Z > 60.f && !TNProcIsSwimming(Turtle))
	{
		// Por arriba: trampolín, como la medusa varada de siempre.
		Turtle->LaunchCharacter(FVector(0.f, 0.f, BounceVelocity), false, true);
	}
	else
	{
		// De lado nadando: picadura que aparta.
		const FVector Away = (Rel * FVector(1.f, 1.f, 0.f)).GetSafeNormal();
		Turtle->LaunchCharacter(Away * StingPush + FVector(0.f, 0.f, 250.f), true, true);
	}
	MulticastBounceFeedback();
}

void ATN_ProcWaterBouncer::MulticastBounceFeedback_Implementation()
{
	SquishAlpha = 1.f;
	if (BounceSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BounceSound, GetActorLocation());
	}
}
