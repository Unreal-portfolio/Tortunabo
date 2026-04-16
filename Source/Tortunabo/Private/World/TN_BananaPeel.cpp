#include "World/TN_BananaPeel.h"
#include "Player/TortugaCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"

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

void ATN_BananaPeel::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void ATN_BananaPeel::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_BananaPeel, bActive);
}

// ── Overlap ────────────────────────────────────────────────────────────────────

void ATN_BananaPeel::OnTriggerBeginOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (!HasAuthority() || !bActive) { return; }

	ATortugaCharacter* Character = Cast<ATortugaCharacter>(OtherActor);
	if (!Character) { return; }

	// Desactivar para que no se active dos veces mientras el personaje todavía overlappea
	bActive = false;
	ApplyActiveState();

	// ── Deslizamiento: mantener XY del momentum actual + componente downward ──
	FVector Velocity = Character->GetVelocity();
	Velocity.Z = 0.f;
	if (Velocity.SizeSquared() < 1.f)
	{
		// Parado: empujar en la dirección opuesta al forward del personaje (resbalón aleatorio)
		Velocity = -Character->GetActorForwardVector();
	}
	Velocity.Normalize();
	const float ImpulseStrength = FMath::Max(Character->GetVelocity().Size2D() * SlideImpulseMultiplier,
	                                          MinSlideForce);
	const FVector SlideImpulse = Velocity * ImpulseStrength + FVector(0.f, 0.f, 200.f);

	// Aplicar knockdown con impulso (ApplyKnockdown gestiona LaunchCharacter internamente en el caracter)
	Character->ApplyKnockdown(KnockdownDuration);
	Character->LaunchCharacter(SlideImpulse, true, false);

	// Feedback visual en todos los clientes
	MulticastOnTriggered();

	// Respawn
	if (RespawnSeconds > 0.f)
	{
		FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &ATN_BananaPeel::Respawn);
		GetWorldTimerManager().SetTimer(RespawnTimerHandle, Del, RespawnSeconds, false);
	}
}

// ── Estado visual ──────────────────────────────────────────────────────────────

void ATN_BananaPeel::ApplyActiveState()
{
	SetActorEnableCollision(bActive);
	if (PeelMesh) { PeelMesh->SetVisibility(bActive); }
}

void ATN_BananaPeel::OnRep_bActive()
{
	ApplyActiveState();
}

void ATN_BananaPeel::Respawn()
{
	bActive = true;
	ApplyActiveState();
}

void ATN_BananaPeel::MulticastOnTriggered_Implementation()
{
	OnPeelTriggered(this);
}
