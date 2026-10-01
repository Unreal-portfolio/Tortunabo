#include "Vehicles/TN_RallyProjectile.h"
#include "Vehicles/TN_Buggy.h"
#include "Vehicles/TN_BuggyMath.h"
#include "Vehicles/TN_RallyTurretLogic.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace TNRallyFX
{
	const TCHAR* const SphereMeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	const TCHAR* const CylinderMeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* const ShapeMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	const FName ColorParameter(TEXT("Color"));

	/** Las mallas básicas miden 100 cm: escala para un radio dado. */
	constexpr float BasicShapeRadiusCm = 50.f;

	/** Segundos que el proyectil no choca con su propio buggy al salir del cañón. */
	constexpr float OwnerIgnoreSeconds = 0.4f;

	float RadiusFor(ETNRallyAmmo Ammo)
	{
		switch (Ammo)
		{
		case ETNRallyAmmo::Burbuja: return 70.f;
		case ETNRallyAmmo::Mortero: return 28.f;
		case ETNRallyAmmo::Alga: return 24.f;
		default: return 18.f;
		}
	}

	FLinearColor ColorFor(ETNRallyAmmo Ammo)
	{
		switch (Ammo)
		{
		case ETNRallyAmmo::Alga: return FLinearColor(0.10f, 0.55f, 0.15f);
		case ETNRallyAmmo::Burbuja: return FLinearColor(0.60f, 0.85f, 1.00f);
		case ETNRallyAmmo::Mortero: return FLinearColor(0.12f, 0.12f, 0.12f);
		case ETNRallyAmmo::Tinta: return FLinearColor(0.05f, 0.02f, 0.10f);
		default: return FLinearColor(0.35f, 0.20f, 0.08f);
		}
	}

	FLinearColor ColorFor(ETNRallyBurstKind Kind)
	{
		switch (Kind)
		{
		case ETNRallyBurstKind::Explosion: return FLinearColor(1.00f, 0.55f, 0.10f);
		case ETNRallyBurstKind::Ink: return FLinearColor(0.05f, 0.02f, 0.10f);
		case ETNRallyBurstKind::BubblePop: return FLinearColor(0.60f, 0.85f, 1.00f);
		case ETNRallyBurstKind::Shield: return FLinearColor(0.40f, 0.90f, 1.00f);
		default: return FLinearColor(0.55f, 0.35f, 0.15f);
		}
	}

	void Tint(UStaticMeshComponent* Mesh, const FLinearColor& Color)
	{
		if (!Mesh)
		{
			return;
		}
		UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
		if (!Mid)
		{
			Mid = Mesh->CreateDynamicMaterialInstance(0);
		}
		if (Mid)
		{
			Mid->SetVectorParameterValue(ColorParameter, Color);
		}
	}

	UStaticMeshComponent* MakeVisual(AActor* Owner, UStaticMesh* Mesh, UMaterialInterface* Material)
	{
		UStaticMeshComponent* Comp = Owner->CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
		Comp->SetStaticMesh(Mesh);
		if (Material)
		{
			Comp->SetMaterial(0, Material);
		}
		Comp->SetCollisionProfileName(TEXT("NoCollision"));
		Comp->SetGenerateOverlapEvents(false);
		Comp->SetCastShadow(false);
		return Comp;
	}
}

// ── Proyectil ─────────────────────────────────────────────────────────────────

ATN_RallyProjectile::ATN_RallyProjectile()
{
	using namespace TNRallyFX;
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->InitSphereRadius(RadiusFor(ETNRallyAmmo::Coco));
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionObjectType(ECC_WorldDynamic);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Block);
	Sphere->SetGenerateOverlapEvents(true);
	Sphere->CanCharacterStepUpOn = ECB_No;
	RootComponent = Sphere;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(SphereMeshPath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(ShapeMaterialPath);
	Mesh = MakeVisual(this, SphereMesh.Object, ShapeMaterial.Object);
	Mesh->SetupAttachment(Sphere);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Sphere;
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;
	// Init da la velocidad en mundo; con el valor por defecto (local) un disparo hacia atrás saldría hacia delante.
	Movement->bInitialVelocityInLocalSpace = false;
	Movement->InitialSpeed = 0.f;
	Movement->MaxSpeed = 0.f;
}

void ATN_RallyProjectile::Init(ETNRallyAmmo InAmmo, const FVector& Velocity, ATN_Buggy* FiredBy)
{
	Ammo = InAmmo;
	Shooter = FiredBy;
	Movement->Velocity = Velocity;
	Movement->ProjectileGravityScale = TNRallyTurret::SpecFor(Ammo).GravityScale;
}

void ATN_RallyProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ATN_RallyProjectile, Ammo, COND_InitialOnly);
}

void ATN_RallyProjectile::BeginPlay()
{
	Super::BeginPlay();
	ApplyLook();
	Movement->ProjectileGravityScale = TNRallyTurret::SpecFor(Ammo).GravityScale;
	SetLifeSpan(TNRallyTurret::SpecFor(Ammo).LifeSeconds);
	if (Ammo == ETNRallyAmmo::Burbuja)
	{
		// Se coge al tocarla: solapa con los buggies en vez de chocar.
		Sphere->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
	}
	// En los clientes Init no ha corrido: el buggy que dispara es el dueño replicado.
	if (!Shooter.IsValid())
	{
		Shooter = Cast<ATN_Buggy>(GetOwner());
	}
	if (!HasAuthority())
	{
		UE_LOG(LogTNBuggy, Verbose, TEXT("Réplica de %s (%s) del buggy %s en (%.0f, %.0f, %.0f)"), *GetName(), *UEnum::GetValueAsString(Ammo),
			*GetNameSafe(GetOwner()), GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);
	}
	if (ATN_Buggy* ShooterBuggy = Shooter.Get())
	{
		Sphere->IgnoreActorWhenMoving(ShooterBuggy, true);
		GetWorldTimerManager().SetTimer(OwnerIgnoreTimer, this, &ATN_RallyProjectile::StopOwnerIgnore, TNRallyFX::OwnerIgnoreSeconds, false);
	}
	if (HasAuthority())
	{
		Sphere->OnComponentHit.AddDynamic(this, &ATN_RallyProjectile::OnSphereHit);
		Sphere->OnComponentBeginOverlap.AddDynamic(this, &ATN_RallyProjectile::OnSphereOverlap);
	}
}

void ATN_RallyProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && !bImpacted)
	{
		const FVector Where = GetActorLocation();
		UE_LOG(LogTNBuggy, Verbose, TEXT("%s sin impacto: acaba en (%.0f, %.0f, %.0f) vel=(%.0f, %.0f, %.0f) gravedad=%.2f simulando=%d"),
			*UEnum::GetValueAsString(Ammo), Where.X, Where.Y, Where.Z, Movement->Velocity.X, Movement->Velocity.Y, Movement->Velocity.Z,
			Movement->ProjectileGravityScale, Movement->UpdatedComponent != nullptr ? 1 : 0);
	}
	Super::EndPlay(EndPlayReason);
}

void ATN_RallyProjectile::StopOwnerIgnore()
{
	if (ATN_Buggy* ShooterBuggy = Shooter.Get())
	{
		Sphere->IgnoreActorWhenMoving(ShooterBuggy, false);
	}
	Shooter.Reset();
	// Una burbuja que ya solapaba con su buggy al acabar la espera: se coge ahora.
	if (HasAuthority() && Ammo == ETNRallyAmmo::Burbuja)
	{
		TArray<AActor*> Overlapping;
		Sphere->GetOverlappingActors(Overlapping, ATN_Buggy::StaticClass());
		if (Overlapping.Num() > 0)
		{
			Impact(Cast<ATN_Buggy>(Overlapping[0]), GetActorLocation());
		}
	}
}

void ATN_RallyProjectile::OnRep_Ammo()
{
	ApplyLook();
}

void ATN_RallyProjectile::ApplyLook()
{
	const float Radius = TNRallyFX::RadiusFor(Ammo);
	Sphere->SetSphereRadius(Radius);
	Mesh->SetRelativeScale3D(FVector(Radius / TNRallyFX::BasicShapeRadiusCm));
	TNRallyFX::Tint(Mesh, TNRallyFX::ColorFor(Ammo));
}

void ATN_RallyProjectile::OnSphereHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	UE_LOG(LogTNBuggy, Verbose, TEXT("%s choca con %s (%s)"), *UEnum::GetValueAsString(Ammo), *GetNameSafe(OtherActor), *GetNameSafe(OtherComp));
	if (Ammo == ETNRallyAmmo::Burbuja && !Cast<ATN_Buggy>(OtherActor))
	{
		// La burbuja se queda flotando donde choca hasta que alguien la coge o se acaba su vida.
		return;
	}
	Impact(Cast<ATN_Buggy>(OtherActor), Hit.ImpactPoint);
}

void ATN_RallyProjectile::OnSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep)
{
	ATN_Buggy* Buggy = Cast<ATN_Buggy>(OtherActor);
	if (!Buggy || Ammo != ETNRallyAmmo::Burbuja || Buggy == Shooter.Get())
	{
		return;
	}
	Impact(Buggy, GetActorLocation());
}

void ATN_RallyProjectile::Impact(ATN_Buggy* HitBuggy, const FVector& Where)
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || bImpacted || !World)
	{
		return;
	}
	bImpacted = true;
	const FVector Dir = Movement->Velocity.GetSafeNormal();
	UE_LOG(LogTNBuggy, Log, TEXT("Impacto de %s en (%.0f, %.0f, %.0f) contra %s"), *UEnum::GetValueAsString(Ammo),
		Where.X, Where.Y, Where.Z, HitBuggy ? *HitBuggy->GetName() : TEXT("el escenario"));

	switch (Ammo)
	{
	case ETNRallyAmmo::Coco:
		if (HitBuggy)
		{
			HitBuggy->ApplyCocoHit(Dir);
		}
		ATN_RallyBurstFX::Spawn(World, ETNRallyBurstKind::CocoHit, Where, 80.f);
		break;
	case ETNRallyAmmo::Alga:
	{
		// El charco va al suelo bajo el impacto.
		FVector Ground = Where;
		FHitResult Down;
		FCollisionQueryParams Params(FName(TEXT("TNRallyPuddle")), false, this);
		if (HitBuggy)
		{
			Params.AddIgnoredActor(HitBuggy);
		}
		if (World->LineTraceSingleByChannel(Down, Where + FVector(0.f, 0.f, 50.f), Where - FVector(0.f, 0.f, 1000.f), ECC_WorldStatic, Params))
		{
			Ground = Down.ImpactPoint;
		}
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<ATN_RallyAlgaPuddle>(ATN_RallyAlgaPuddle::StaticClass(), FTransform(Ground), Spawn);
		break;
	}
	case ETNRallyAmmo::Burbuja:
		if (HitBuggy)
		{
			HitBuggy->GrantShield();
		}
		ATN_RallyBurstFX::Spawn(World, ETNRallyBurstKind::BubblePop, Where, 160.f);
		break;
	case ETNRallyAmmo::Mortero:
		for (TActorIterator<ATN_Buggy> It(World); It; ++It)
		{
			if (FVector::Dist(It->GetActorLocation(), Where) <= TNRallyTurret::MortarRadiusCm)
			{
				It->ApplyMortarBlast();
			}
		}
		ATN_RallyBurstFX::Spawn(World, ETNRallyBurstKind::Explosion, Where, TNRallyTurret::MortarRadiusCm);
		break;
	case ETNRallyAmmo::Tinta:
		if (HitBuggy)
		{
			HitBuggy->ApplyInk();
		}
		ATN_RallyBurstFX::Spawn(World, ETNRallyBurstKind::Ink, Where, 150.f);
		break;
	default:
		break;
	}
	Destroy();
}

// ── Charco de alga ────────────────────────────────────────────────────────────

ATN_RallyAlgaPuddle::ATN_RallyAlgaPuddle()
{
	using namespace TNRallyFX;
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(CylinderMeshPath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(ShapeMaterialPath);
	Mesh = MakeVisual(this, CylinderMesh.Object, ShapeMaterial.Object);
	RootComponent = Mesh;
	const float Scale = TNRallyTurret::AlgaPuddleRadiusCm / BasicShapeRadiusCm;
	// Disco de 6 m de radio y 4 cm de alto, apoyado en el suelo.
	Mesh->SetRelativeScale3D(FVector(Scale, Scale, 0.04f));
}

void ATN_RallyAlgaPuddle::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(TNRallyTurret::AlgaPuddleSeconds);
	TNRallyFX::Tint(Mesh, TNRallyFX::ColorFor(ETNRallyAmmo::Alga));
}

void ATN_RallyAlgaPuddle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	CheckAccumulator += DeltaSeconds;
	if (CheckAccumulator < 0.1f)
	{
		return;
	}
	CheckAccumulator = 0.f;
	const FVector Center = GetActorLocation();
	for (TActorIterator<ATN_Buggy> It(GetWorld()); It; ++It)
	{
		ATN_Buggy* Buggy = *It;
		const FVector Delta = Buggy->GetActorLocation() - Center;
		if (FVector(Delta.X, Delta.Y, 0.f).Size() > TNRallyTurret::AlgaPuddleRadiusCm || FMath::Abs(Delta.Z) > 300.f)
		{
			continue;
		}
		if (Immune.Contains(Buggy))
		{
			continue;
		}
		if (Buggy->TryConsumeShield())
		{
			Immune.Add(Buggy);
			continue;
		}
		Buggy->NotePuddleContact();
	}
}

// ── Ráfaga cosmética ──────────────────────────────────────────────────────────

ATN_RallyBurstFX::ATN_RallyBurstFX()
{
	using namespace TNRallyFX;
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(SphereMeshPath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(ShapeMaterialPath);
	Mesh = MakeVisual(this, SphereMesh.Object, ShapeMaterial.Object);
	RootComponent = Mesh;
}

ATN_RallyBurstFX* ATN_RallyBurstFX::Spawn(UWorld* World, ETNRallyBurstKind InKind, const FVector& Where, float InRadiusCm)
{
	if (!World)
	{
		return nullptr;
	}
	ATN_RallyBurstFX* Burst = World->SpawnActorDeferred<ATN_RallyBurstFX>(ATN_RallyBurstFX::StaticClass(), FTransform(Where), nullptr,
		nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Burst)
	{
		Burst->Kind = InKind;
		Burst->RadiusCm = InRadiusCm;
		Burst->FinishSpawning(FTransform(Where));
	}
	return Burst;
}

void ATN_RallyBurstFX::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ATN_RallyBurstFX, Kind, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATN_RallyBurstFX, RadiusCm, COND_InitialOnly);
}

void ATN_RallyBurstFX::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(0.6f);
	ApplyLook();
}

void ATN_RallyBurstFX::OnRep_Look()
{
	ApplyLook();
}

void ATN_RallyBurstFX::ApplyLook()
{
	TNRallyFX::Tint(Mesh, TNRallyFX::ColorFor(Kind));
	Mesh->SetRelativeScale3D(FVector(0.2f * RadiusCm / TNRallyFX::BasicShapeRadiusCm));
}

void ATN_RallyBurstFX::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float Alpha = FMath::Clamp(Age / 0.5f, 0.f, 1.f);
	// Crece rápido hasta el radio final y se encoge al final para desaparecer.
	const float Grow = 1.f - FMath::Square(1.f - Alpha);
	const float Shrink = Alpha > 0.8f ? 1.f - (Alpha - 0.8f) / 0.2f : 1.f;
	Mesh->SetRelativeScale3D(FVector(FMath::Max(0.01f, Grow * Shrink) * RadiusCm / TNRallyFX::BasicShapeRadiusCm));
}
