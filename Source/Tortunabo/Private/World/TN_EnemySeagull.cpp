#include "World/TN_EnemySeagull.h"
#include "Player/TortugaCharacter.h"
#include "Core/TN_CoopPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"

ATN_EnemySeagull::ATN_EnemySeagull()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SeagullMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SeagullMesh"));
	SeagullMesh->SetupAttachment(SceneRoot);
	SeagullMesh->SetIsReplicated(false);
	SeagullMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	DangerDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("DangerDecal"));
	DangerDecal->SetupAttachment(SceneRoot);
	// Apuntar hacia abajo para proyectar en el suelo
	DangerDecal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	DangerDecal->DecalSize = FVector(DecalDepth, MaxDangerRadius, MaxDangerRadius);
}

void ATN_EnemySeagull::BeginPlay()
{
	Super::BeginPlay();
	CountdownRemaining = AttackTimerSeconds;
	// Solo el servidor ejecuta la lógica de juego
	SetActorTickEnabled(HasAuthority());
}

void ATN_EnemySeagull::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
	TargetCharacter.Reset();
	TargetController.Reset();
	Super::EndPlay(EndPlayReason);
}

void ATN_EnemySeagull::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_EnemySeagull, CountdownRemaining);
	DOREPLIFETIME(ATN_EnemySeagull, TargetPlayerIndex);
	DOREPLIFETIME(ATN_EnemySeagull, StunRemaining);
	DOREPLIFETIME(ATN_EnemySeagull, BlindRemaining);
}

// ── API Pública ────────────────────────────────────────────────────────────────

void ATN_EnemySeagull::InitializeWithTarget(ATortugaCharacter* Target)
{
	if (!HasAuthority() || !Target) { return; }

	// Alinear tamaño de decal en frame 1 antes de cualquier otra lógica —
	// evita el pop visual entre el default del ctor y el primer tick.
	CountdownRemaining = AttackTimerSeconds;
	UpdateDecalSize();

	TargetCharacter  = Target;
	TargetController = Cast<APlayerController>(Target->GetController());

	// Resolver índice para que los clientes puedan identificar al objetivo
	if (AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr)
	{
		for (int32 i = 0; i < GS->PlayerArray.Num(); ++i)
		{
			if (GS->PlayerArray[i] && GS->PlayerArray[i]->GetPawn() == Target)
			{
				TargetPlayerIndex = i;
				break;
			}
		}
	}

	const FVector StartLoc = Target->GetActorLocation() + FVector(0.f, 0.f, FollowHeight);
	SetActorLocation(StartLoc);

	UE_LOG(LogTemp, Log, TEXT("[EnemySeagull] Initialized on '%s' — countdown %.1fs"),
		*GetNameSafe(Target), AttackTimerSeconds);
}

float ATN_EnemySeagull::GetCurrentDangerRadius() const
{
	if (AttackTimerSeconds <= 0.f) { return MinKillRadius; }
	const float NormT = FMath::Clamp(CountdownRemaining / AttackTimerSeconds, 0.f, 1.f);
	return FMath::Lerp(MinKillRadius, MaxDangerRadius, NormT);
}

// ── Tick principal ─────────────────────────────────────────────────────────────

void ATN_EnemySeagull::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!HasAuthority()) { return; }

	// Stun congela completamente la IA. El countdown del ataque no avanza.
	if (StunRemaining > 0.f)
	{
		StunRemaining = FMath::Max(0.f, StunRemaining - DeltaTime);
		return;
	}
	if (BlindRemaining > 0.f)
	{
		BlindRemaining = FMath::Max(0.f, BlindRemaining - DeltaTime);
		// Ciega: pierde target → retreat. Solo dispara una vez al inicio del blind.
		if (!bAttackResolved && !bIsRetreating && !bIsStriking)
		{
			AbortAndRetreat();
			return;
		}
	}

	// Máquinas de estado tienen prioridad — early-out garantiza que solo una corre
	if (bIsStriking)
	{
		TickStrike(DeltaTime);
		return;
	}
	if (bIsRetreating)
	{
		TickRetreat(DeltaTime);
		return;
	}
	if (bAttackResolved) { return; }

	TickFollowTarget(DeltaTime);
	TickEscapeCheck(DeltaTime);
	TickRoofCheck(DeltaTime);
	TickCountdown(DeltaTime);
}

// ── Sub-ticks del estado de seguimiento ───────────────────────────────────────

void ATN_EnemySeagull::TickFollowTarget(float DeltaTime)
{
	ATortugaCharacter* Target = TargetCharacter.Get();
	if (!Target) { return; }

	const FVector TargetLoc  = Target->GetActorLocation() + FVector(0.f, 0.f, FollowHeight);
	const FVector CurrentLoc = GetActorLocation();
	const float   Dist       = FVector::Dist(CurrentLoc, TargetLoc);
	if (Dist > 1.f)
	{
		const FVector Dir = (TargetLoc - CurrentLoc).GetSafeNormal();
		SetActorLocation(CurrentLoc + Dir * FMath::Min(FollowSpeed * DeltaTime, Dist));
	}
}

void ATN_EnemySeagull::TickCountdown(float DeltaTime)
{
	if (bAttackResolved) { return; }

	CountdownRemaining -= DeltaTime;
	UpdateDecalSize();

	if (CountdownRemaining <= 0.f)
	{
		CountdownRemaining = 0.f;
		ResolveAttack();
	}
}

void ATN_EnemySeagull::TickEscapeCheck(float DeltaTime)
{
	if (bAttackResolved) { return; }

	// TargetPlayerIndex == -1 → InitializeWithTarget aún no fue llamado
	if (TargetPlayerIndex == -1) { return; }

	ATortugaCharacter* Target = TargetCharacter.Get();
	if (!Target)
	{
		AbortAndRetreat();
		return;
	}

	const float DistXY = FVector::Dist2D(GetActorLocation(), Target->GetActorLocation());
	if (DistXY > GetCurrentDangerRadius())
	{
		TimeOutsideShadow += DeltaTime;
		if (TimeOutsideShadow >= EscapeSeconds)
		{
			AbortAndRetreat();
		}
	}
	else
	{
		TimeOutsideShadow = 0.f;
	}
}

void ATN_EnemySeagull::TickRoofCheck(float DeltaTime)
{
	if (bAttackResolved) { return; }

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextRoofCheckTime) { return; }
	NextRoofCheckTime = Now + RoofCheckInterval;

	if (HasRoofBetweenSeagullAndTarget())
	{
		AbortAndRetreat();
	}
}

// ── Máquina de estado: picotazo físico ────────────────────────────────────────

void ATN_EnemySeagull::TickStrike(float DeltaTime)
{
	if (bStrikeGoingDown)
	{
		StrikeAlpha += DeltaTime / FMath::Max(StrikeDuration, KINDA_SMALL_NUMBER);

		const float ClampedAlpha = FMath::Min(StrikeAlpha, 1.f);
		const float NewZ = FMath::Lerp(StrikeStartZ, StrikeTargetZ, ClampedAlpha);
		SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, NewZ));

		if (StrikeAlpha >= 1.f)
		{
			// Punto de impacto alcanzado — resolver daño
			ATortugaCharacter* Target = TargetCharacter.Get();
			MulticastPlayStrikeEffects(GetActorLocation());

			if (Target)
			{
				if (Target->HasUmbrellaProtection())
				{
					// Sombrilla bloquea el golpe; la gaviota retrocede sin matar
					MulticastPlayRetreatEffect();
				}
				else if (Target->HasBigHeadActive())
				{
					Target->RemoveBigHeadEffect();
					MulticastPlayBigHeadAbsorbEffect(Target);
				}
				else
				{
					MulticastPlayKillEffect(Target);
					Target->RequestKill(this);
				}
			}

			// Iniciar subida de vuelta
			bStrikeGoingDown = false;
			StrikeAlpha      = 0.f;
			RetreatStartZ    = StrikeTargetZ;
			RetreatEndZ      = StrikeStartZ + RetreatHeight;
		}
	}
	else
	{
		// Fase de subida tras el picotazo
		StrikeAlpha += DeltaTime / FMath::Max(RiseDuration, KINDA_SMALL_NUMBER);

		const float ClampedAlpha = FMath::Min(StrikeAlpha, 1.f);
		const float NewZ = FMath::Lerp(RetreatStartZ, RetreatEndZ, ClampedAlpha);
		SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, NewZ));

		if (StrikeAlpha >= 1.f)
		{
			bIsStriking = false;
			SetLifeSpan(0.2f); // Dar tiempo a que los Multicasts lleguen a clientes
		}
	}
}

// ── Máquina de estado: retirada ───────────────────────────────────────────────

void ATN_EnemySeagull::TickRetreat(float DeltaTime)
{
	StrikeAlpha += DeltaTime / FMath::Max(RiseDuration, KINDA_SMALL_NUMBER);

	const float ClampedAlpha = FMath::Min(StrikeAlpha, 1.f);
	const float NewZ = FMath::Lerp(RetreatStartZ, RetreatEndZ, ClampedAlpha);
	SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, NewZ));

	if (StrikeAlpha >= 1.f)
	{
		bIsRetreating = false;
		SetLifeSpan(0.2f);
	}
}

// ── Lógica de ataque / retirada ───────────────────────────────────────────────

void ATN_EnemySeagull::ResolveAttack()
{
	if (bAttackResolved) { return; }
	bAttackResolved = true;

	ATortugaCharacter* Target = TargetCharacter.Get();

	if (!Target)
	{
		AbortAndRetreat();
		return;
	}

	// Cubierta detectada en el último check periódico → abortar
	if (HasRoofBetweenSeagullAndTarget())
	{
		AbortAndRetreat();
		return;
	}

	// Jugador fuera del radio de kill → escapó en el último momento
	const float DistXY = FVector::Dist2D(GetActorLocation(), Target->GetActorLocation());
	if (DistXY > MinKillRadius)
	{
		AbortAndRetreat();
		return;
	}

	// Iniciar picotazo físico
	bIsStriking      = true;
	bStrikeGoingDown = true;
	StrikeAlpha      = 0.f;
	StrikeStartZ     = GetActorLocation().Z;
	StrikeTargetZ    = Target->GetActorLocation().Z;
}

void ATN_EnemySeagull::AbortAndRetreat()
{
	if (bIsRetreating || bIsStriking) { return; }

	bIsRetreating   = true;
	bAttackResolved = true;
	StrikeAlpha     = 0.f;
	RetreatStartZ   = GetActorLocation().Z;
	RetreatEndZ     = RetreatStartZ + RetreatHeight;

	MulticastPlayRetreatEffect();
}

bool ATN_EnemySeagull::HasRoofBetweenSeagullAndTarget() const
{
	ATortugaCharacter* Target = TargetCharacter.Get();
	if (!Target) { return false; }

	FHitResult Hit;
	FCollisionQueryParams Params(TEXT("SeagullRoofCheck"), false);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(Target);

	GetWorld()->LineTraceSingleByChannel(
		Hit,
		GetActorLocation(),
		Target->GetActorLocation(),
		ECC_WorldStatic,
		Params
	);

	return Hit.bBlockingHit;
}

// ── Visual ────────────────────────────────────────────────────────────────────

void ATN_EnemySeagull::UpdateDecalSize()
{
	if (!DangerDecal) { return; }
	const float R = GetCurrentDangerRadius();
	DangerDecal->DecalSize = FVector(DecalDepth, R, R);
}

// ── OnRep ─────────────────────────────────────────────────────────────────────

void ATN_EnemySeagull::OnRep_CountdownRemaining()
{
	// Clientes actualizan el visual del decal cuando llega el valor replicado
	UpdateDecalSize();
}

void ATN_EnemySeagull::OnRep_TargetPlayerIndex()
{
	if (!GetWorld()) { return; }
	AGameStateBase* GS = GetWorld()->GetGameState();
	if (!GS || !GS->PlayerArray.IsValidIndex(TargetPlayerIndex)) { return; }

	if (APlayerState* PS = GS->PlayerArray[TargetPlayerIndex])
	{
		TargetCharacter = Cast<ATortugaCharacter>(PS->GetPawn());
	}
}

// ── Multicast — efectos visuales/sonoros ──────────────────────────────────────

void ATN_EnemySeagull::MulticastPlayStrikeEffects_Implementation(FVector TargetLocation)
{
	if (StrikeVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, StrikeVFX, TargetLocation);
	}
	if (StrikeSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, StrikeSound, TargetLocation);
	}
}

void ATN_EnemySeagull::MulticastPlayRetreatEffect_Implementation()
{
	if (RetreatSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, RetreatSound, GetActorLocation());
	}
}

void ATN_EnemySeagull::MulticastPlayKillEffect_Implementation(ATortugaCharacter* Victim)
{
	if (!Victim) { return; }
	if (KillSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, KillSound, Victim->GetActorLocation());
	}
}

void ATN_EnemySeagull::MulticastPlayBigHeadAbsorbEffect_Implementation(ATortugaCharacter* Target)
{
	if (!Target) { return; }
	if (BigHeadAbsorbSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, BigHeadAbsorbSound, Target->GetActorLocation());
	}
}

// ── ITN_EnemyTargetInterface ──────────────────────────────────────────────────

void ATN_EnemySeagull::ApplyStun(float Duration)
{
	if (!HasAuthority() || Duration <= 0.f) { return; }
	StunRemaining = FMath::Max(StunRemaining, Duration);
	UE_LOG(LogTemp, Log, TEXT("[STUN] Seagull '%s' stunned for %.2fs"), *GetName(), Duration);
	MulticastPlayStunEffect(Duration);
}

void ATN_EnemySeagull::ApplyBlind(float Duration)
{
	if (!HasAuthority() || Duration <= 0.f) { return; }
	BlindRemaining = FMath::Max(BlindRemaining, Duration);
	UE_LOG(LogTemp, Log, TEXT("[BLIND] Seagull '%s' blinded for %.2fs"), *GetName(), Duration);
}

void ATN_EnemySeagull::OnRep_StunRemaining()
{
	// Cliente recibe inicio del stun → spawn VFX local (multicast es respaldo).
}

void ATN_EnemySeagull::MulticastPlayStunEffect_Implementation(float Duration)
{
	if (StunSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, StunSound, GetActorLocation());
	}
	if (StunVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, StunVFX, GetActorLocation());
	}
}
