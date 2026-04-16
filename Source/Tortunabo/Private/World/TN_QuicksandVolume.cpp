#include "World/TN_QuicksandVolume.h"
#include "Player/TortugaCharacter.h"
#include "Player/TN_StaminaComponent.h"
#include "Core/TN_CoopPlayerState.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ATN_QuicksandVolume::ATN_QuicksandVolume()
{
	// Sin tick propio — usamos un timer de servidor
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // El volumen no se replica: lógica local + servidor autoritario

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);
	TriggerBox->SetBoxExtent(FVector(300.f, 300.f, 100.f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void ATN_QuicksandVolume::BeginPlay()
{
	Super::BeginPlay();

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ATN_QuicksandVolume::OnBoxBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ATN_QuicksandVolume::OnBoxEndOverlap);
}

void ATN_QuicksandVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
	}
	ActiveChars.Empty();
	Super::EndPlay(EndPlayReason);
}

// ── Overlap ───────────────────────────────────────────────────────────────────

void ATN_QuicksandVolume::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	ATortugaCharacter* Char = Cast<ATortugaCharacter>(OtherActor);
	if (!IsValid(Char)) { return; }

	// ── Efectos locales en TODAS las máquinas (igual que SlowZoneVolume) ─────────
	FSandState State;

	// Speed cap — CMC predice correctamente al usar el mismo valor en servidor y cliente
	if (UTN_StaminaComponent* Stamina = Char->FindComponentByClass<UTN_StaminaComponent>())
	{
		Stamina->SetSpeedCap(SlowSpeed);
	}

	// Aumentar gravedad + deshabilitar salto: cachear originales para restaurar al salir
	if (UCharacterMovementComponent* CMC = Char->GetCharacterMovement())
	{
		State.OrigGravityScale = CMC->GravityScale;
		State.OrigJumpZVel     = CMC->JumpZVelocity;

		CMC->GravityScale  = 3.f;
		CMC->JumpZVelocity = 50.f; // Prácticamente imposible salir
	}

	Char->OnDestroyed.AddUniqueDynamic(this, &ATN_QuicksandVolume::OnCharacterDestroyed);

	// Registrar en todas las máquinas para poder restaurar CMC en EndOverlap
	ActiveChars.FindOrAdd(TWeakObjectPtr<ATortugaCharacter>(Char)) = State;

	// ── Lógica de muerte: solo servidor ──────────────────────────────────────────
	if (!HasAuthority()) { return; }

	const ATN_CoopPlayerState* PS = Char->GetPlayerState<ATN_CoopPlayerState>();
	if (!PS || !PS->bIsAlive || PS->bIsEliminated) { return; }

	StartSandTick();
}

void ATN_QuicksandVolume::OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ATortugaCharacter* Char = Cast<ATortugaCharacter>(OtherActor);
	if (!IsValid(Char)) { return; }

	// ── Restaurar efectos locales en TODAS las máquinas ───────────────────────────
	if (UTN_StaminaComponent* Stamina = Char->FindComponentByClass<UTN_StaminaComponent>())
	{
		Stamina->ClearSpeedCap();
	}

	// Restaurar valores originales del CMC cacheados en BeginOverlap
	if (UCharacterMovementComponent* CMC = Char->GetCharacterMovement())
	{
		if (const FSandState* State = ActiveChars.Find(TWeakObjectPtr<ATortugaCharacter>(Char)))
		{
			CMC->GravityScale  = State->OrigGravityScale;
			CMC->JumpZVelocity = State->OrigJumpZVel;
		}
	}

	Char->OnDestroyed.RemoveDynamic(this, &ATN_QuicksandVolume::OnCharacterDestroyed);
	ActiveChars.Remove(TWeakObjectPtr<ATortugaCharacter>(Char));

	// ── Servidor: parar tick si no queda nadie ────────────────────────────────────
	if (HasAuthority() && ActiveChars.IsEmpty())
	{
		StopSandTick();
	}
}

void ATN_QuicksandVolume::OnCharacterDestroyed(AActor* DestroyedActor)
{
	ATortugaCharacter* Char = Cast<ATortugaCharacter>(DestroyedActor);
	if (!Char) { return; }

	// Limpieza sin restaurar (el CMC ya no existe)
	if (HasAuthority())
	{
		ActiveChars.Remove(TWeakObjectPtr<ATortugaCharacter>(Char));
		if (ActiveChars.IsEmpty()) { StopSandTick(); }
	}
}

// ── Server sand tick ──────────────────────────────────────────────────────────

void ATN_QuicksandVolume::StartSandTick()
{
	if (!HasAuthority() || GetWorld()->GetTimerManager().IsTimerActive(SandTickHandle)) { return; }

	FTimerDelegate Delegate;
	Delegate.BindUObject(this, &ATN_QuicksandVolume::ServerSandTick);
	GetWorld()->GetTimerManager().SetTimer(SandTickHandle, Delegate, SandTickInterval, true);
}

void ATN_QuicksandVolume::StopSandTick()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(SandTickHandle);
	}
}

void ATN_QuicksandVolume::ServerSandTick()
{
	if (!HasAuthority()) { return; }

	TArray<TWeakObjectPtr<ATortugaCharacter>> ToRemove;

	for (auto& Pair : ActiveChars)
	{
		ATortugaCharacter* Char = Pair.Key.Get();
		if (!IsValid(Char))
		{
			ToRemove.Add(Pair.Key);
			continue;
		}

		const ATN_CoopPlayerState* PS = Char->GetPlayerState<ATN_CoopPlayerState>();
		if (!PS || !PS->bIsAlive || PS->bIsEliminated)
		{
			ToRemove.Add(Pair.Key);
			continue;
		}

		Pair.Value.TimeInSand += SandTickInterval;

		// Impulso descendente en cada tick — crea el efecto de hundimiento gradual.
		// bZOverride=false: se suma a la velocidad actual (no teleporta).
		// bXYOverride=false: no interfiere con el movimiento horizontal.
		// La gravedad alta (3x) + impulso hacia abajo presionan al personaje contra el suelo.
		Char->LaunchCharacter(FVector(0.f, 0.f, -SinkForce), false, false);

		// Muerte por hundimiento total
		if (Pair.Value.TimeInSand >= SinkDuration)
		{
			Char->RequestKill(this);
			ToRemove.Add(Pair.Key);
		}
	}

	for (const TWeakObjectPtr<ATortugaCharacter>& Key : ToRemove)
	{
		ActiveChars.Remove(Key);
	}

	if (ActiveChars.IsEmpty())
	{
		StopSandTick();
	}
}
