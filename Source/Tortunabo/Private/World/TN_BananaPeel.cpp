#include "World/TN_BananaPeel.h"
#include "Core/TN_Log.h"
#include "Player/TortugaCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"

ATN_BananaPeel::ATN_BananaPeel()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	PeelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PeelMesh"));
	SetRootComponent(PeelMesh);
	PeelMesh->SetIsReplicated(false);
	PeelMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PeelMesh->SetCollisionObjectType(ECC_WorldDynamic);
	PeelMesh->SetCollisionResponseToAllChannels(ECR_Block);
	PeelMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(PeelMesh);
	TriggerBox->SetBoxExtent(FVector(40.f, 40.f, 20.f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
}

void ATN_BananaPeel::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ATN_BananaPeel::OnTriggerBeginOverlap);
	}
}

// ── Overlap ────────────────────────────────────────────────────────────────────

void ATN_BananaPeel::OnTriggerBeginOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (!HasAuthority() || bTriggered) { return; }

	bTriggered = true;

	ATortugaCharacter* Character = Cast<ATortugaCharacter>(OtherActor);
	if (!Character) { bTriggered = false; return; }

	// ── Deslizamiento: preferir el momentum actual; si está parado, usar
	// la dirección en que mira el personaje (resbalón clásico hacia delante).
	// Antes el fallback era -ActorForwardVector → el jugador salía hacia atrás,
	// que es el bug Q4-03 reportado: "el plátano no desliza hacia delante".
	FVector SlideDir = Character->GetVelocity();
	SlideDir.Z = 0.f;
	if (SlideDir.SizeSquared() < 1.f)
	{
		SlideDir = Character->GetActorForwardVector();
	}
	SlideDir = SlideDir.GetSafeNormal();

	const float ImpulseStrength = FMath::Max(
		Character->GetVelocity().Size2D() * SlideImpulseMultiplier,
		MinSlideForce);

	// Componente Z alto para que LaunchCharacter meta al CMC en MOVE_Falling —
	// sin contacto con el suelo la fricción no mata el slide. Tunable desde BP.
	const FVector SlideImpulse = SlideDir * ImpulseStrength + FVector(0.f, 0.f, SlideVerticalForce);

	UE_LOG(LogTortunabo, Log, TEXT("[BananaPeel] %s hit peel — slide dir=(%.2f,%.2f,%.2f) mag=%.0f z=%.0f"),
		*GetNameSafe(Character), SlideDir.X, SlideDir.Y, SlideDir.Z, ImpulseStrength, SlideVerticalForce);

	Character->ApplyKnockdown(KnockdownDuration, SlideImpulse);

	// VFX/audio en todos los clientes antes de destruir
	MulticastOnTriggered(GetActorLocation());

	// SetLifeSpan garantiza que el Multicast llegue a los clientes antes de la destrucción
	SetLifeSpan(0.2f);
}

// ── Multicast ─────────────────────────────────────────────────────────────────

void ATN_BananaPeel::MulticastOnTriggered_Implementation(FVector Location)
{
	if (TriggerVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, TriggerVFX, Location);
	}
	if (TriggerSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, TriggerSound, Location);
	}
}
