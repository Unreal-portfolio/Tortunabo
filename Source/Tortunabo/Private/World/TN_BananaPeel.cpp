#include "World/TN_BananaPeel.h"
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

	// ── Deslizamiento: mantener XY del momentum actual + componente upward ──
	FVector Velocity = Character->GetVelocity();
	Velocity.Z = 0.f;
	if (Velocity.SizeSquared() < 1.f)
	{
		// Parado: empujar en la dirección opuesta al forward (resbalón)
		Velocity = -Character->GetActorForwardVector();
	}
	Velocity.Normalize();
	const float ImpulseStrength = FMath::Max(Character->GetVelocity().Size2D() * SlideImpulseMultiplier,
	                                          MinSlideForce);
	const FVector SlideImpulse = Velocity * ImpulseStrength + FVector(0.f, 0.f, 200.f);

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
