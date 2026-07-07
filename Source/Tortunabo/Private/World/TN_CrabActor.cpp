#include "World/TN_CrabActor.h"
#include "Core/TN_Log.h"
#include "Player/TortugaCharacter.h"
#include "Core/TN_CoopPlayerState.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"

ATN_CrabActor::ATN_CrabActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);

	CrabMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CrabMesh"));
	SetRootComponent(CrabMesh);
	CrabMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CrabMesh->SetIsReplicated(false);

	DetectionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("DetectionSphere"));
	DetectionSphere->SetupAttachment(CrabMesh);
	DetectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DetectionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	DetectionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// Cuerpo físico para items. Channel ECC_Pawn + Block to WorldDynamic.
	// Matriz UE5: la respuesta resultante entre dos componentes es la MÁS PERMISIVA
	// (Ignore < Overlap < Block).
	//   Concha OverlapSphere (WorldDynamic, Pawn=Overlap) ↔ BodyCollision (Pawn,
	//     WorldDynamic=Block) → min(Overlap,Block)=Overlap → overlap event en concha ✓
	//   Throwable Ball Mesh (WorldDynamic, BlockAllDynamic con Pawn=Block) ↔
	//     BodyCollision (Pawn, WorldDynamic=Block) → min(Block,Block)=Block →
	//     OnComponentHit dispara en la bola ✓
	// Mi intento anterior con Overlap a WorldDynamic generaba min(Block,Overlap)=
	// Overlap y la bola NUNCA hacía Hit (commit anterior tenía un comentario
	// erróneo justificando un comportamiento que UE5 no entrega).
	BodyCollision = CreateDefaultSubobject<USphereComponent>(TEXT("BodyCollision"));
	BodyCollision->SetupAttachment(CrabMesh);
	BodyCollision->InitSphereRadius(BodyCollisionRadius);
	BodyCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BodyCollision->SetCollisionObjectType(ECC_Pawn);
	BodyCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	BodyCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	BodyCollision->SetGenerateOverlapEvents(true);
	BodyCollision->SetNotifyRigidBodyCollision(true);
}

void ATN_CrabActor::BeginPlay()
{
	Super::BeginPlay();

	// Solo el servidor hace tick de lógica
	SetActorTickEnabled(HasAuthority());

	if (!HasAuthority()) { return; }

	// Diferir 1 tick: compatibilidad con chunks (Child Actor Components terminan
	// de posicionarse después de BeginPlay)
	FTimerDelegate Delegate;
	Delegate.BindUObject(this, &ATN_CrabActor::InitializePatrolPoints);
	GetWorldTimerManager().SetTimer(InitTimerHandle, Delegate, 0.05f, false);

	DetectionSphere->SetSphereRadius(DetectionRadius);
	BodyCollision->SetSphereRadius(BodyCollisionRadius);
	// OnDetectionBeginOverlap se registra en InitializePatrolPoints (deferred 1 tick)
	// para garantizar que SpawnLocation está inicializado antes de que pueda dispararse.
}

void ATN_CrabActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
	ChaseTarget.Reset();
	Super::EndPlay(EndPlayReason);
}

void ATN_CrabActor::InitializePatrolPoints()
{
	SpawnLocation = GetActorLocation();
	WorldPatrolPoints.Reset();

	for (const FVector& Offset : PatrolPoints)
	{
		WorldPatrolPoints.Add(SpawnLocation + Offset);
	}

	if (WorldPatrolPoints.IsEmpty())
	{
		WorldPatrolPoints.Add(SpawnLocation);
	}

	// Registrar el overlap AHORA que SpawnLocation está inicializado.
	// Si lo registráramos en BeginPlay, podría dispararse en el tick 0 antes de que
	// SpawnLocation esté seteado, causando un MaxChaseDistance check contra (0,0,0).
	DetectionSphere->OnComponentBeginOverlap.AddDynamic(
		this, &ATN_CrabActor::OnDetectionBeginOverlap);
}

void ATN_CrabActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_CrabActor, CrabState);
	DOREPLIFETIME(ATN_CrabActor, StunRemaining);
	DOREPLIFETIME(ATN_CrabActor, BlindRemaining);
}

// ── Tick ─────────────────────────────────────────────────────────────────────

void ATN_CrabActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!HasAuthority()) { return; }

	// Stun: pausa toda la IA. Cangrejo queda inmóvil hasta que expira.
	if (StunRemaining > 0.f)
	{
		StunRemaining = FMath::Max(0.f, StunRemaining - DeltaTime);
		return;
	}
	// Blind: pierde target de chase y vuelve a patrol. Detection sphere ignorada
	// vía guard en OnDetectionBeginOverlap.
	if (BlindRemaining > 0.f)
	{
		BlindRemaining = FMath::Max(0.f, BlindRemaining - DeltaTime);
		if (CrabState == ETNCrabState::Chase || CrabState == ETNCrabState::Attack)
		{
			ChaseTarget.Reset();
			SetCrabState(ETNCrabState::Patrol);
		}
	}

	switch (CrabState)
	{
	case ETNCrabState::Patrol:   TickPatrol(DeltaTime);   break;
	case ETNCrabState::Chase:    TickChase(DeltaTime);    break;
	case ETNCrabState::Attack:   TickAttack();            break;
	case ETNCrabState::Cooldown: TickCooldown(DeltaTime); break;
	}
}

// ── Patrol ───────────────────────────────────────────────────────────────────

void ATN_CrabActor::TickPatrol(float DeltaTime)
{
	if (WorldPatrolPoints.Num() <= 1) { return; }

	const FVector& Target = WorldPatrolPoints[CurrentPatrolIndex];
	MoveTowards(Target, PatrolSpeed, DeltaTime);

	if (FVector::Dist2D(GetActorLocation(), Target) < 50.f)
	{
		CurrentPatrolIndex = (CurrentPatrolIndex + 1) % WorldPatrolPoints.Num();
	}
}

// ── Chase ─────────────────────────────────────────────────────────────────────

void ATN_CrabActor::TickChase(float DeltaTime)
{
	ATortugaCharacter* Target = ChaseTarget.Get();

	if (!IsAliveAndValid(Target))
	{
		ChaseTarget.Reset();
		SetCrabState(ETNCrabState::Patrol);
		return;
	}

	// Abandonar si el jugador salió de la zona delimitada del cangrejo
	const float DistTargetFromSpawn = FVector::Dist2D(Target->GetActorLocation(), SpawnLocation);
	if (DistTargetFromSpawn > MaxChaseDistance)
	{
		ChaseTarget.Reset();
		CurrentPatrolIndex = FindNearestPatrolIndex();
		SetCrabState(ETNCrabState::Patrol);
		return;
	}

	const float DistToTarget = FVector::Dist2D(GetActorLocation(), Target->GetActorLocation());
	if (DistToTarget <= AttackRadius)
	{
		SetCrabState(ETNCrabState::Attack);
		return;
	}

	MoveTowards(Target->GetActorLocation(), ChaseSpeed, DeltaTime);
}

// ── Attack ───────────────────────────────────────────────────────────────────

void ATN_CrabActor::TickAttack()
{
	ATortugaCharacter* Target = ChaseTarget.Get();
	if (IsAliveAndValid(Target))
	{
		Target->RequestKill(this);
	}

	CooldownRemaining = CooldownDuration;
	ChaseTarget.Reset();
	SetCrabState(ETNCrabState::Cooldown);
}

// ── Cooldown ─────────────────────────────────────────────────────────────────

void ATN_CrabActor::TickCooldown(float DeltaTime)
{
	CooldownRemaining -= DeltaTime;
	if (CooldownRemaining <= 0.f)
	{
		CurrentPatrolIndex = FindNearestPatrolIndex();
		SetCrabState(ETNCrabState::Patrol);
	}
}

// ── Helpers ──────────────────────────────────────────────────────────────────

void ATN_CrabActor::MoveTowards(const FVector& Target, float Speed, float DeltaTime)
{
	const FVector CurrentLoc = GetActorLocation();
	const FVector Dir        = (Target - CurrentLoc).GetSafeNormal2D();
	const float   Step       = Speed * DeltaTime;
	const float   Dist       = FVector::Dist2D(CurrentLoc, Target);

	SetActorLocation(CurrentLoc + Dir * FMath::Min(Step, Dist));

	if (!Dir.IsNearlyZero())
	{
		SetActorRotation(Dir.Rotation());
	}
}

int32 ATN_CrabActor::FindNearestPatrolIndex() const
{
	int32 Best = 0;
	float BestDist = MAX_FLT;
	for (int32 i = 0; i < WorldPatrolPoints.Num(); ++i)
	{
		const float D = FVector::Dist2D(GetActorLocation(), WorldPatrolPoints[i]);
		if (D < BestDist) { BestDist = D; Best = i; }
	}
	return Best;
}

bool ATN_CrabActor::IsAliveAndValid(ATortugaCharacter* Char) const
{
	if (!IsValid(Char)) { return false; }
	const ATN_CoopPlayerState* PS = Char->GetPlayerState<ATN_CoopPlayerState>();
	return PS && PS->bIsAlive && !PS->bIsEliminated;
}

void ATN_CrabActor::SetCrabState(ETNCrabState NewState)
{
	if (CrabState == NewState) { return; }
	CrabState = NewState;
	OnRep_CrabState();
}

void ATN_CrabActor::OnRep_CrabState()
{
	// Reproducir montage y sonido directamente — sin Blueprint
	UAnimInstance* AnimInst = CrabMesh ? CrabMesh->GetAnimInstance() : nullptr;

	UAnimMontage* Montage = nullptr;
	USoundBase*   Sound   = nullptr;

	switch (CrabState)
	{
	case ETNCrabState::Patrol:
		Montage = PatrolMontage;
		Sound   = PatrolSound;
		break;
	case ETNCrabState::Chase:
		Montage = ChaseMontage;
		Sound   = ChaseSound;
		break;
	case ETNCrabState::Attack:
		Montage = AttackMontage;
		Sound   = AttackSound;
		break;
	case ETNCrabState::Cooldown:
		Montage = CooldownMontage;
		Sound   = CooldownSound;
		break;
	}

	if (AnimInst && Montage)
	{
		AnimInst->Montage_Play(Montage);
	}
	if (Sound)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, Sound, GetActorLocation());
	}
}

// ── Overlap de detección ──────────────────────────────────────────────────────

void ATN_CrabActor::OnDetectionBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	// No interrumpir si ya está en cooldown, persiguiendo, aturdido o cegado
	if (CrabState == ETNCrabState::Chase || CrabState == ETNCrabState::Attack) { return; }
	if (CooldownRemaining > 0.f) { return; }
	if (StunRemaining > 0.f || BlindRemaining > 0.f) { return; }

	ATortugaCharacter* Char = Cast<ATortugaCharacter>(OtherActor);
	if (!IsAliveAndValid(Char)) { return; }

	// Solo perseguir si el jugador está dentro de la zona del cangrejo
	const float DistFromSpawn = FVector::Dist2D(Char->GetActorLocation(), SpawnLocation);
	if (DistFromSpawn > MaxChaseDistance) { return; }

	ChaseTarget = Char;
	SetCrabState(ETNCrabState::Chase);
}

// ── ITN_EnemyTargetInterface ──────────────────────────────────────────────────

void ATN_CrabActor::ApplyStun(float Duration)
{
	if (!HasAuthority() || Duration <= 0.f) { return; }
	StunRemaining = FMath::Max(StunRemaining, Duration);
	UE_LOG(LogTortunabo, Log, TEXT("[STUN] Crab '%s' stunned for %.2fs"), *GetName(), Duration);
	MulticastPlayStunEffect(Duration);
}

void ATN_CrabActor::ApplyBlind(float Duration)
{
	if (!HasAuthority() || Duration <= 0.f) { return; }
	BlindRemaining = FMath::Max(BlindRemaining, Duration);
	UE_LOG(LogTortunabo, Log, TEXT("[BLIND] Crab '%s' blinded for %.2fs"), *GetName(), Duration);
	if (CrabState == ETNCrabState::Chase || CrabState == ETNCrabState::Attack)
	{
		ChaseTarget.Reset();
		SetCrabState(ETNCrabState::Patrol);
	}
}

void ATN_CrabActor::MulticastPlayStunEffect_Implementation(float Duration)
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
