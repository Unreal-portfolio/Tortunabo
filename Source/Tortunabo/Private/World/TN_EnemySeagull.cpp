#include "World/TN_EnemySeagull.h"
#include "Player/TortugaCharacter.h"
#include "Core/TN_CoopPlayerState.h"
#include "Game/TN_RunGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"

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

	// Solo el servidor hace tick de lógica de juego
	SetActorTickEnabled(HasAuthority());
}

void ATN_EnemySeagull::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TargetCharacter.Reset();
	TargetController.Reset();
	Super::EndPlay(EndPlayReason);
}

void ATN_EnemySeagull::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_EnemySeagull, CountdownRemaining);
	DOREPLIFETIME(ATN_EnemySeagull, TargetPlayerIndex);
}

// ── API Pública ────────────────────────────────────────────────────────────────

void ATN_EnemySeagull::InitializeWithTarget(ATortugaCharacter* Target)
{
	if (!HasAuthority() || !Target) { return; }

	TargetCharacter = Target;
	TargetController = Cast<APlayerController>(Target->GetController());

	// Resolver TargetPlayerIndex para replicar a clientes
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

	// Spawnear directamente encima del objetivo
	const FVector StartLoc = Target->GetActorLocation() + FVector(0.f, 0.f, FollowHeight);
	SetActorLocation(StartLoc);

	CountdownRemaining = AttackTimerSeconds;
	UpdateDecalSize();

	UE_LOG(LogTemp, Log, TEXT("[EnemySeagull] Spawned on '%s' — countdown %.1fs"),
		*GetNameSafe(Target), AttackTimerSeconds);
}

// ── Tick ────────────────────────────────────────────────────────────────────────

void ATN_EnemySeagull::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority() || bAttackResolved) { return; }

	TickFollowTarget(DeltaTime);
	TickCountdown(DeltaTime);
}

void ATN_EnemySeagull::TickFollowTarget(float DeltaTime)
{
	ATortugaCharacter* Target = TargetCharacter.Get();
	if (!Target) { return; }

	// Destino: encima del objetivo en Z
	const FVector TargetLoc = Target->GetActorLocation() + FVector(0.f, 0.f, FollowHeight);
	const FVector CurrentLoc = GetActorLocation();
	const FVector Dir = (TargetLoc - CurrentLoc).GetSafeNormal();
	const float StepDist = FollowSpeed * DeltaTime;

	// Mover suavemente — no teletransportar si ya está cerca
	const float DistToTarget = FVector::Dist(CurrentLoc, TargetLoc);
	if (DistToTarget > 1.f)
	{
		const FVector NewLoc = CurrentLoc + Dir * FMath::Min(StepDist, DistToTarget);
		SetActorLocation(NewLoc);
	}
}

void ATN_EnemySeagull::TickCountdown(float DeltaTime)
{
	CountdownRemaining -= DeltaTime;
	UpdateDecalSize();

	if (CountdownRemaining <= 0.f)
	{
		CountdownRemaining = 0.f;
		ResolveAttack();
	}
}

void ATN_EnemySeagull::ResolveAttack()
{
	if (bAttackResolved) { return; }
	bAttackResolved = true;

	ATortugaCharacter* Target = TargetCharacter.Get();

	// Sin objetivo válido (murió antes) → gaviota simplemente desaparece
	if (!Target)
	{
		Destroy();
		return;
	}

	const float DistXY = FVector::Dist2D(GetActorLocation(), Target->GetActorLocation());
	const bool bInKillZone = (DistXY <= MinKillRadius);

	if (bInKillZone)
	{
		// ── Sombrilla bloquea el ataque (#29) ──────────────────────────────────
		if (Target->HasUmbrellaProtection())
		{
			UE_LOG(LogTemp, Log, TEXT("[EnemySeagull] Umbrella blocked attack on '%s'"), *GetNameSafe(Target));
			MulticastResolveResult(false, false, Target);  // Missed visually
			Destroy();
			return;
		}

		// ── Big Head absorbe el golpe (#2) ─────────────────────────────────────
		if (Target->HasBigHeadActive())
		{
			Target->RemoveBigHeadEffect();
			MulticastResolveResult(false, true, Target);
			Destroy();
			return;
		}

		// ── Matar al jugador ───────────────────────────────────────────────────
		ATN_RunGameMode* GM = Cast<ATN_RunGameMode>(GetWorld()->GetAuthGameMode());
		if (GM && TargetController.IsValid())
		{
			MulticastResolveResult(true, false, Target);
			GM->MarkPlayerDead(TargetController.Get());
		}
	}
	else
	{
		// Escapó — feedback visual
		MulticastResolveResult(false, false, Target);
	}

	Destroy();
}

// ── Visual ────────────────────────────────────────────────────────────────────

float ATN_EnemySeagull::GetCurrentDangerRadius() const
{
	if (AttackTimerSeconds <= 0.f) { return MinKillRadius; }
	const float NormT = FMath::Clamp(CountdownRemaining / AttackTimerSeconds, 0.f, 1.f);
	return FMath::Lerp(MinKillRadius, MaxDangerRadius, NormT);
}

void ATN_EnemySeagull::UpdateDecalSize()
{
	if (!DangerDecal) { return; }
	const float R = GetCurrentDangerRadius();
	DangerDecal->DecalSize = FVector(DecalDepth, R, R);
}

void ATN_EnemySeagull::OnRep_CountdownRemaining()
{
	// En clientes: actualizar visual del decal cuando llega el valor replicado
	UpdateDecalSize();
}

void ATN_EnemySeagull::OnRep_TargetPlayerIndex()
{
	// Clientes resuelven la referencia local al personaje objetivo (para eventos BP)
	if (!GetWorld()) { return; }
	AGameStateBase* GS = GetWorld()->GetGameState();
	if (!GS || !GS->PlayerArray.IsValidIndex(TargetPlayerIndex)) { return; }

	if (APlayerState* PS = GS->PlayerArray[TargetPlayerIndex])
	{
		TargetCharacter = Cast<ATortugaCharacter>(PS->GetPawn());
	}
}

// ── Multicast resultado ───────────────────────────────────────────────────────

void ATN_EnemySeagull::MulticastResolveResult_Implementation(bool bKilled, bool bBigHeadAbsorbed,
	ATortugaCharacter* VictimOrEscapee)
{
	if (bBigHeadAbsorbed)
	{
		OnBigHeadAbsorbed(VictimOrEscapee);
	}
	else if (bKilled)
	{
		OnSeagullKill(VictimOrEscapee);
	}
	else
	{
		OnSeagullMissed(VictimOrEscapee);
	}
}

// ── Spawn helper estático ─────────────────────────────────────────────────────

ATN_EnemySeagull* ATN_EnemySeagull::SpawnOnRandomPlayer(const UObject* WorldContextObject,
	TSubclassOf<ATN_EnemySeagull> SeagullClass)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World || !SeagullClass) { return nullptr; }

	// Recopilar todos los personajes vivos
	TArray<ATortugaCharacter*> AlivePlayers;
	for (TActorIterator<ATortugaCharacter> It(World); It; ++It)
	{
		ATortugaCharacter* C = *It;
		if (!C || !C->GetController()) { continue; }

		if (ATN_CoopPlayerState* PS = C->GetPlayerState<ATN_CoopPlayerState>())
		{
			if (PS->bIsAlive && !PS->bIsEliminated)
			{
				AlivePlayers.Add(C);
			}
		}
	}

	if (AlivePlayers.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemySeagull] SpawnOnRandomPlayer: no alive players found."));
		return nullptr;
	}

	// Seleccionar aleatoriamente
	const int32 Idx = FMath::RandRange(0, AlivePlayers.Num() - 1);
	ATortugaCharacter* Target = AlivePlayers[Idx];

	const FVector SpawnLoc = Target->GetActorLocation() + FVector(0.f, 0.f, 400.f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATN_EnemySeagull* Seagull = World->SpawnActor<ATN_EnemySeagull>(SeagullClass, SpawnLoc,
		FRotator::ZeroRotator, Params);

	if (Seagull)
	{
		Seagull->InitializeWithTarget(Target);
	}

	return Seagull;
}
