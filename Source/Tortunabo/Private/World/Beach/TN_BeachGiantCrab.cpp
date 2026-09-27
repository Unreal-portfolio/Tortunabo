#include "World/Beach/TN_BeachGiantCrab.h"
#include "World/Beach/TN_BeachCameraShake.h"
#include "World/Beach/TN_BeachEnemySynth.h"
#include "World/Beach/TN_BeachStun.h"
#include "World/TN_EnemyDecisions.h"
#include "TN_BeachEnemyKit.h"
#include "TN_BeachEnemyMeshes.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Player/TortugaCharacter.h"

namespace TNBeachCrab
{
	/** Estados del cangrejo gigante (Mover.State). */
	enum class EState : uint8 { Patrol, Idle, Chase, WindUp, Slam, Recover, Return, Dazed };

	inline uint8 ToByte(EState E)
	{
		return static_cast<uint8>(E);
	}

	/** Zona de patrulla: esta fracción de la huella alrededor de su sitio. */
	constexpr float PatrolFraction = 0.52f;
	/** Ve a una tortuga a esta distancia del cuerpo (cm, por el tamaño). */
	constexpr float DetectRadius = 2200.f;
	/** La deja si la tortuga se aleja de su sitio más que esto (o 1,4 veces la huella). */
	constexpr float LeashRadius = 3800.f;
	/** Velocidades (cm/s, por el tamaño): paseo, persecución (tortuga: 450 andando, 800 esprintando) y vuelta a casa. */
	constexpr float PatrolSpeed = 260.f;
	constexpr float ChaseSpeed = 560.f;
	constexpr float ReturnSpeed = 400.f;
	/** Lo que se recoloca durante el aviso para que la pinza caiga donde marca la sombra. */
	constexpr float WindUpSpeed = 650.f;
	/** Empieza el mazazo si la tortuga está a su alcance más esto. */
	constexpr float AttackSlack = 300.f;
	/** Aviso (pinza en alto temblando), caída, pinza clavada y espera hasta el siguiente mazazo (s). */
	constexpr float WindUpTime = 0.6f;
	constexpr float SlamTime = 0.14f;
	constexpr float RecoverTime = 1.1f;
	constexpr float CooldownTime = 1.4f;
	/** Radio del golpe alrededor de la sombra (cm, por el tamaño). */
	constexpr float HitRadius = 280.f;
	/** Aturdimiento y tiempo que ignora a la golpeada. */
	constexpr float StunSeconds = 3.5f;
	constexpr float IgnoreSeconds = 6.f;
	/** Giro (grados/s) y adelanto al apuntar (s de la velocidad de la tortuga). */
	constexpr float TurnRate = 240.f;
	constexpr float LeadSeconds = 0.3f;
	/** Cabeceo del brazo con la pinza en alto. */
	constexpr float RaisePitch = 72.f;
	/** Largo de una zancada (cm) para la marcha de las patas. */
	constexpr float Stride = 180.f;

	inline float TurnToward(float From, float To, float MaxStep)
	{
		return FMath::UnwindDegrees(From + FMath::Clamp(FMath::FindDeltaAngleDegrees(From, To), -MaxStep, MaxStep));
	}

	inline float Smooth01(float X)
	{
		const float C = FMath::Clamp(X, 0.f, 1.f);
		return C * C * (3.f - 2.f * C);
	}
}

ATN_BeachGiantCrab::ATN_BeachGiantCrab()
{
	SetNetUpdateFrequency(12.f);
}

void ATN_BeachGiantCrab::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_BeachGiantCrab, StunEndTime);
	DOREPLIFETIME(ATN_BeachGiantCrab, BlindEndTime);
}

// ─────────────────────────────────────────────────────────────────────────────
// Construcción
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachGiantCrab::ApplySpec()
{
	SizeK = FMath::Clamp(Spec.SizeScale, 0.8f, 1.2f);
	const float Footprint = GetFootprintRadius();
	PatrolRadius = Footprint * TNBeachCrab::PatrolFraction;
	DetectRadius = TNBeachCrab::DetectRadius * SizeK;
	LeashRadius = FMath::Max(TNBeachCrab::LeashRadius * SizeK, Footprint * 1.4f);
	Reach = static_cast<float>(TNBeachMeshes::CrabRig().Reach) * SizeK;
	GroundZ = static_cast<float>(GetActorLocation().Z);
	BuildCrab();
}

void ATN_BeachGiantCrab::BuildCrab()
{
	if (BodyBlock)
	{
		return;
	}
	USceneComponent* RigRoot = MakeRig();
	const TNBeachMeshes::FCrabRig R = TNBeachMeshes::CrabRig();

	Scaler = NewObject<USceneComponent>(this, TEXT("CrabScaler"));
	Scaler->SetupAttachment(RigRoot);
	Scaler->SetRelativeScale3D(FVector(SizeK));
	Scaler->RegisterComponent();

	// Cuerpo sólido en todas las máquinas: la tortuga (y su bola) no lo atraviesan. Tipo Pawn que bloquea lo dinámico,
	// como el cangrejo de siempre, para que los objetos del jugador le den (ITN_EnemyTargetInterface).
	BodyBlock = NewObject<UBoxComponent>(this, TEXT("CrabBlock"));
	BodyBlock->SetupAttachment(Scaler);
	BodyBlock->SetBoxExtent(FVector(TNBeachMeshes::CrabD * TNBeach::Scale, TNBeachMeshes::CrabW * TNBeach::Scale * 1.02, R.BodyZ * 0.75), false);
	BodyBlock->SetRelativeLocation(FVector(0.0, 0.0, R.BodyZ * 0.75));
	BodyBlock->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BodyBlock->SetCollisionObjectType(ECC_Pawn);
	BodyBlock->SetCollisionResponseToAllChannels(ECR_Ignore);
	BodyBlock->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	BodyBlock->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	BodyBlock->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	BodyBlock->SetCanEverAffectNavigation(false);
	BodyBlock->SetGenerateOverlapEvents(true);
	BodyBlock->RegisterComponent();

	if (!bHasScreen)
	{
		return;
	}

	const TNBeachMeshes::FCrabLook Look = TNBeachMeshes::CrabPalette(Spec.Seed);
	const FString Pal = FString::Printf(TEXT("Beach.Crab.%d."), ((Spec.Seed % 4) + 4) % 4);
	using TNProcMesh::FTNProcMeshBuffers;
	UStaticMesh* BodyMesh = TNBeachKit::CachedMesh(Pal + TEXT("Body"), [&Look](FTNProcMeshBuffers& M) { TNBeachMeshes::BuildCrabBody(M, Look); });
	UStaticMesh* EyeMesh = TNBeachKit::CachedMesh(Pal + TEXT("Eye"), [&Look](FTNProcMeshBuffers& M) { TNBeachMeshes::BuildCrabEye(M, Look); });
	UStaticMesh* LegL = TNBeachKit::CachedMesh(Pal + TEXT("LegL"), [&Look](FTNProcMeshBuffers& M) { TNBeachMeshes::BuildCrabLeg(M, Look, -1.0); });
	UStaticMesh* LegR = TNBeachKit::CachedMesh(Pal + TEXT("LegR"), [&Look](FTNProcMeshBuffers& M) { TNBeachMeshes::BuildCrabLeg(M, Look, 1.0); });
	UStaticMesh* ArmMesh = TNBeachKit::CachedMesh(Pal + TEXT("Arm"), [&Look, &R](FTNProcMeshBuffers& M) { TNBeachMeshes::BuildCrabBigArm(M, Look, R); });
	UStaticMesh* HandMesh = TNBeachKit::CachedMesh(Pal + TEXT("Hand"), [&Look](FTNProcMeshBuffers& M) { TNBeachMeshes::BuildCrabBigHand(M, Look); });
	UStaticMesh* FingerMesh = TNBeachKit::CachedMesh(Pal + TEXT("Finger"), [&Look](FTNProcMeshBuffers& M) { TNBeachMeshes::BuildCrabBigFinger(M, Look); });
	UStaticMesh* SmallMesh = TNBeachKit::CachedMesh(Pal + TEXT("Small"), [&Look](FTNProcMeshBuffers& M) { TNBeachMeshes::BuildCrabSmallClaw(M, Look); });

	Body = TNBeachKit::AddPart(this, Scaler, BodyMesh, FVector(0.0, 0.0, R.BodyZ));
	Eyes.Add(TNBeachKit::AddPart(this, Body, EyeMesh, R.EyeL, false));
	Eyes.Add(TNBeachKit::AddPart(this, Body, EyeMesh, R.EyeR, false));
	for (int32 k = 0; k < 8; ++k)
	{
		UStaticMeshComponent* Leg = TNBeachKit::AddPart(this, Body, k < 4 ? LegL : LegR, R.LegPivot[k]);
		if (Leg)
		{
			Leg->SetRelativeRotation(FRotator(0.f, R.LegSplay[k], 0.f));
		}
		Legs.Add(Leg);
	}
	BigArm = TNBeachKit::AddPart(this, Body, ArmMesh, R.BigShoulder);
	BigHand = TNBeachKit::AddPart(this, BigArm, HandMesh, R.BigElbow);
	BigFinger = TNBeachKit::AddPart(this, BigHand, FingerMesh, R.BigKnuckle);
	SmallClaw = TNBeachKit::AddPart(this, Body, SmallMesh, R.SmallShoulder);
	ClawShadow = TNBeachKit::AddShadow(this, 0.6f);
	TNBeachKit::PlaceShadow(ClawShadow, FVector::ZeroVector, 0.f);

	using TNAmbientFX::EShape;
	TNAmbientFX::FEmitterDesc FoamDesc = TNBeachKit::MakeDesc(EShape::Puff, FLinearColor(0.96f, 0.98f, 1.f), true, 0.85f, 40, 4.f, 90.f, 0.f, 1.2f, 2.4f, 45.f, 95.f);
	FoamDesc.Buoyancy = 60.f;
	FoamDesc.Spread = 0.7f;
	FoamDesc.SpawnRadius = 60.f;
	TNBeachKit::InitEmitter(Foam, this, FoamDesc, static_cast<uint32>(Spec.Seed) + 11u);
	TNAmbientFX::FEmitterDesc PuffDesc = TNBeachKit::MakeDesc(EShape::Puff, FLinearColor(0.86f, 0.76f, 0.56f), true, 0.55f, 30, 0.f, 650.f, -100.f, 1.2f, 2.2f, 220.f, 520.f);
	PuffDesc.Drag = 1.2f;
	PuffDesc.Spread = 0.9f;
	PuffDesc.SpawnRadius = 150.f;
	TNBeachKit::InitEmitter(SandPuff, this, PuffDesc, static_cast<uint32>(Spec.Seed) + 12u);
	TNAmbientFX::FEmitterDesc GrainDesc = TNBeachKit::MakeDesc(EShape::Ember, FLinearColor(0.8f, 0.7f, 0.5f), false, 1.f, 40, 0.f, 1300.f, -1600.f, 0.8f, 1.4f, 26.f, 20.f);
	GrainDesc.Spread = 0.85f;
	GrainDesc.SpawnRadius = 120.f;
	TNBeachKit::InitEmitter(SandGrains, this, GrainDesc, static_cast<uint32>(Spec.Seed) + 13u);

	GetVoice(Body, 1200.f, 9000.f);
	LastShown = ShownLoc;
}

// ─────────────────────────────────────────────────────────────────────────────
// Servidor
// ─────────────────────────────────────────────────────────────────────────────

FVector ATN_BeachGiantCrab::PickPatrolGoal()
{
	const float Angle = ServerRng.FRandRange(0.f, 2.f * PI);
	const float Dist = PatrolRadius * FMath::Sqrt(ServerRng.FRand());
	return Home + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);
}

FVector ATN_BeachGiantCrab::StepToward(const FVector& Goal, float Speed, float DeltaSeconds, FVector& OutDir)
{
	FVector Flat = Goal - SimLoc;
	Flat.Z = 0.0;
	const double Dist = Flat.Size();
	OutDir = Dist > 1.0 ? Flat / Dist : FVector::ZeroVector;
	FVector Next = SimLoc + OutDir * FMath::Min(Dist, static_cast<double>(Speed * DeltaSeconds));
	// Nunca más lejos de su sitio que la correa.
	FVector FromHome = Next - Home;
	FromHome.Z = 0.0;
	if (FromHome.Size() > LeashRadius)
	{
		Next = Home + FromHome.GetSafeNormal() * LeashRadius;
	}
	GroundTimer -= DeltaSeconds;
	if (GroundTimer <= 0.f)
	{
		GroundTimer = 0.1f;
		float Z = GroundZ;
		if (TraceGround(this, Next, Z))
		{
			GroundZ = Z;
		}
	}
	Next.Z = FMath::FInterpTo(SimLoc.Z, static_cast<double>(GroundZ), static_cast<double>(DeltaSeconds), 8.0);
	return Next;
}

void ATN_BeachGiantCrab::ServerWalk(const FVector& Goal, float Speed, float DeltaSeconds, bool bSideways)
{
	FVector Dir;
	const FVector Next = StepToward(Goal, Speed, DeltaSeconds, Dir);
	float WantYaw = SimYaw;
	if (!Dir.IsNearlyZero())
	{
		const float Heading = static_cast<float>(Dir.Rotation().Yaw);
		if (bSideways)
		{
			// De lado: hacia el costado que menos haya que girar (como los cangrejos de la fauna).
			const float Right = Heading - 90.f;
			const float Left = Heading + 90.f;
			WantYaw = FMath::Abs(FMath::FindDeltaAngleDegrees(SimYaw, Right)) <= FMath::Abs(FMath::FindDeltaAngleDegrees(SimYaw, Left)) ? Right : Left;
		}
		else
		{
			WantYaw = Heading;
		}
	}
	ServerMoveTo(Next, TNBeachCrab::TurnToward(SimYaw, WantYaw, TNBeachCrab::TurnRate * DeltaSeconds));
}

void ATN_BeachGiantCrab::StartWindUp(ATortugaCharacter* Victim)
{
	using TNBeachCrab::EState;
	const FVector Vel = Victim->GetVelocity();
	FVector Impact = Victim->GetActorLocation() + FVector(Vel.X, Vel.Y, 0.0) * TNBeachCrab::LeadSeconds;
	// Como mucho, a donde llega la pinza desde donde puede colocarse durante el aviso.
	FVector FromMe = Impact - SimLoc;
	FromMe.Z = 0.0;
	const float MaxReach = Reach + TNBeachCrab::WindUpSpeed * SizeK * TNBeachCrab::WindUpTime * 0.8f;
	if (FromMe.Size() > MaxReach)
	{
		Impact = SimLoc + FromMe.GetSafeNormal() * MaxReach;
	}
	float Z = static_cast<float>(Victim->GetActorLocation().Z);
	if (TraceGround(this, Impact, Z))
	{
		Impact.Z = Z;
	}
	else
	{
		Impact.Z = SimLoc.Z;
	}
	ServerSetState(TNBeachCrab::ToByte(EState::WindUp), Impact);
}

void ATN_BeachGiantCrab::ResolveSlam()
{
	const FVector Impact = Mover.Aim;
	bool bHit = false;
	if (IsRaceLive(this))
	{
		TArray<ATortugaCharacter*> Turtles;
		GatherTurtles(this, Turtles);
		const float Radius = TNBeachCrab::HitRadius * SizeK + 40.f;
		for (ATortugaCharacter* Turtle : Turtles)
		{
			if (!IsValid(Turtle) || TNBeach::IsTurtleStunned(Turtle))
			{
				continue;
			}
			const FVector At = Turtle->GetActorLocation();
			if (FVector::Dist2D(At, Impact) > Radius || FMath::Abs(At.Z - Impact.Z) > 450.0)
			{
				continue;
			}
			FVector Away = At - Impact;
			Away.Z = 0.0;
			Away = Away.GetSafeNormal();
			// Despachurrada: casi en el sitio, un empujoncito hacia fuera.
			StunTurtle(Turtle, TNBeachCrab::StunSeconds, Away * 320.0 + FVector(0.0, 0.0, 160.0));
			IgnoreTurtle(Turtle, TNBeachCrab::IgnoreSeconds);
			bHit = true;
		}
	}
	MulticastSlam(Impact, bHit);
}

void ATN_BeachGiantCrab::ServerTick(float DeltaSeconds)
{
	using TNBeachCrab::EState;
	using TNBeachCrab::ToByte;
	const EState State = static_cast<EState>(GetMoverState());
	AttackCooldown -= DeltaSeconds;
	StateLeft -= DeltaSeconds;

	if (IsStunned())
	{
		if (State != EState::Dazed)
		{
			Target.Reset();
			ServerSetState(ToByte(EState::Dazed));
		}
		return;
	}
	const bool bLive = IsRaceLive(this);
	const bool bCanSee = bLive && !IsBlinded();

	switch (State)
	{
	case EState::Dazed:
		Target.Reset();
		ServerSetState(ToByte(EState::Return));
		break;

	case EState::Patrol:
	case EState::Idle:
	{
		if (bCanSee)
		{
			if (ATortugaCharacter* Seen = FindTarget(SimLoc, DetectRadius, Home, LeashRadius))
			{
				Target = Seen;
				ServerSetState(ToByte(EState::Chase));
				break;
			}
		}
		if (State == EState::Patrol)
		{
			ServerWalk(PatrolGoal, TNBeachCrab::PatrolSpeed * SizeK, DeltaSeconds, true);
			if (FVector::Dist2D(SimLoc, PatrolGoal) < 120.0)
			{
				StateLeft = ServerRng.FRandRange(1.5f, 3.5f);
				ServerSetState(ToByte(EState::Idle));
			}
		}
		else if (StateLeft <= 0.f)
		{
			PatrolGoal = PickPatrolGoal();
			ServerSetState(ToByte(EState::Patrol));
		}
		break;
	}

	case EState::Chase:
	{
		ATortugaCharacter* Victim = Target.Get();
		const bool bValid = bCanSee && IsTargetable(Victim);
		const float FromHome = bValid ? static_cast<float>(FVector::Dist2D(Victim->GetActorLocation(), Home)) : 0.f;
		const float ToVictim = bValid ? static_cast<float>(FVector::Dist2D(Victim->GetActorLocation(), SimLoc)) : 0.f;
		// La misma decisión (probada) que el cangrejo de siempre, con las medidas del gigante.
		switch (TNCrabLogic::DecideChaseTransition(bValid, FromHome, LeashRadius, ToVictim, Reach + TNBeachCrab::AttackSlack * SizeK))
		{
		case TNCrabLogic::EChaseTransition::ReturnToPatrol_TargetLost:
			Target = bCanSee ? FindTarget(SimLoc, DetectRadius, Home, LeashRadius) : nullptr;
			if (!Target.IsValid())
			{
				ServerSetState(ToByte(EState::Return));
			}
			break;
		case TNCrabLogic::EChaseTransition::ReturnToPatrol_OutOfZone:
			Target.Reset();
			ServerSetState(ToByte(EState::Return));
			break;
		case TNCrabLogic::EChaseTransition::StartAttack:
			if (AttackCooldown <= 0.f)
			{
				StartWindUp(Victim);
			}
			else
			{
				ServerWalk(Victim->GetActorLocation(), TNBeachCrab::ChaseSpeed * SizeK * 0.5f, DeltaSeconds, true);
			}
			break;
		case TNCrabLogic::EChaseTransition::KeepChasing:
			ServerWalk(Victim->GetActorLocation(), TNBeachCrab::ChaseSpeed * SizeK, DeltaSeconds, true);
			break;
		}
		break;
	}

	case EState::WindUp:
	{
		// Se recoloca para que la pinza caiga en la sombra y la encara.
		const FVector Impact = Mover.Aim;
		FVector Dir = Impact - SimLoc;
		Dir.Z = 0.0;
		Dir = Dir.IsNearlyZero() ? FRotator(0.f, SimYaw, 0.f).Vector() : Dir.GetSafeNormal();
		FVector MoveDir;
		const FVector Next = StepToward(Impact - Dir * Reach, TNBeachCrab::WindUpSpeed * SizeK, DeltaSeconds, MoveDir);
		const float Facing = static_cast<float>(Dir.Rotation().Yaw);
		ServerMoveTo(Next, TNBeachCrab::TurnToward(SimYaw, Facing, 420.f * DeltaSeconds));
		if (GetStateAge() >= TNBeachCrab::WindUpTime)
		{
			ServerSetState(ToByte(EState::Slam), Impact);
		}
		break;
	}

	case EState::Slam:
		if (GetStateAge() >= TNBeachCrab::SlamTime)
		{
			ResolveSlam();
			AttackCooldown = TNBeachCrab::CooldownTime;
			ServerSetState(ToByte(EState::Recover), FVector(Mover.Aim));
		}
		break;

	case EState::Recover:
		if (GetStateAge() >= TNBeachCrab::RecoverTime)
		{
			ATortugaCharacter* Next = Target.Get();
			if (!bCanSee || !IsTargetable(Next) || FVector::Dist2D(Next->GetActorLocation(), Home) > LeashRadius)
			{
				Next = bCanSee ? FindTarget(SimLoc, DetectRadius, Home, LeashRadius) : nullptr;
			}
			Target = Next;
			ServerSetState(ToByte(Next ? EState::Chase : EState::Return));
		}
		break;

	case EState::Return:
	default:
	{
		if (bCanSee)
		{
			if (ATortugaCharacter* Seen = FindTarget(SimLoc, DetectRadius * 0.8f, Home, LeashRadius * 0.8f))
			{
				Target = Seen;
				ServerSetState(ToByte(EState::Chase));
				break;
			}
		}
		ServerWalk(Home, TNBeachCrab::ReturnSpeed * SizeK, DeltaSeconds, true);
		if (FVector::Dist2D(SimLoc, Home) < 200.0)
		{
			PatrolGoal = PickPatrolGoal();
			StateLeft = 1.5f;
			ServerSetState(ToByte(EState::Idle));
		}
		break;
	}
	}

	if (IsDebugDraw())
	{
		UWorld* World = GetWorld();
		DrawDebugCircle(World, SimLoc + FVector(0.0, 0.0, 40.0), DetectRadius, 48, FColor::Yellow, false, -1.f, 0, 10.f, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), false);
		DrawDebugCircle(World, Home + FVector(0.0, 0.0, 40.0), LeashRadius, 64, FColor::Orange, false, -1.f, 0, 10.f, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), false);
		DrawDebugCircle(World, Home + FVector(0.0, 0.0, 40.0), PatrolRadius, 48, FColor::Green, false, -1.f, 0, 6.f, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), false);
		if (State == EState::WindUp || State == EState::Slam)
		{
			DrawDebugCircle(World, FVector(Mover.Aim) + FVector(0.0, 0.0, 40.0), TNBeachCrab::HitRadius * SizeK, 32, FColor::Red, false, -1.f, 0, 12.f, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), false);
		}
		if (const ATortugaCharacter* Victim = Target.Get())
		{
			DrawDebugLine(World, SimLoc + FVector(0.0, 0.0, 300.0), Victim->GetActorLocation(), FColor::Red, false, -1.f, 0, 8.f);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Objetos del jugador (ITN_EnemyTargetInterface)
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachGiantCrab::ApplyStun(float Duration)
{
	if (!HasAuthority() || Duration <= 0.f)
	{
		return;
	}
	const float Now = static_cast<float>(ServerNow(this));
	StunEndTime = TNCrabLogic::ExtendEffectEndTime(StunEndTime, Now, Duration);
	Target.Reset();
	ServerSetState(TNBeachCrab::ToByte(TNBeachCrab::EState::Dazed));
}

void ATN_BeachGiantCrab::ApplyBlind(float Duration)
{
	if (!HasAuthority() || Duration <= 0.f)
	{
		return;
	}
	const float Now = static_cast<float>(ServerNow(this));
	BlindEndTime = TNCrabLogic::ExtendEffectEndTime(BlindEndTime, Now, Duration);
	const TNBeachCrab::EState State = static_cast<TNBeachCrab::EState>(GetMoverState());
	if (State == TNBeachCrab::EState::Chase || State == TNBeachCrab::EState::WindUp)
	{
		Target.Reset();
		ServerSetState(TNBeachCrab::ToByte(TNBeachCrab::EState::Return));
	}
}

bool ATN_BeachGiantCrab::IsStunned() const
{
	return TNCrabLogic::ComputeEffectRemaining(StunEndTime, static_cast<float>(ServerNow(this))) > 0.f;
}

bool ATN_BeachGiantCrab::IsBlinded() const
{
	return TNCrabLogic::ComputeEffectRemaining(BlindEndTime, static_cast<float>(ServerNow(this))) > 0.f;
}

// ─────────────────────────────────────────────────────────────────────────────
// Visual
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachGiantCrab::OnMoverStateChanged(uint8 OldState)
{
	if (!bHasScreen || !Voice)
	{
		return;
	}
	using TNBeachCrab::EState;
	const EState State = static_cast<EState>(GetMoverState());
	const float Pitch = 1.f / FMath::Sqrt(SizeK);
	switch (State)
	{
	case EState::Chase:
		if (static_cast<EState>(OldState) != EState::Recover)
		{
			Voice->Play(ETNBeachSfx::Clack, Pitch, 1.f);
		}
		break;
	case EState::WindUp:
		Voice->Play(ETNBeachSfx::Clack, Pitch * 0.8f, 1.2f);
		break;
	case EState::Dazed:
		Voice->Play(ETNBeachSfx::Clack, Pitch * 0.6f, 0.8f);
		break;
	default:
		break;
	}
}

void ATN_BeachGiantCrab::MulticastSlam_Implementation(FVector_NetQuantize Where, bool bHit)
{
	if (!bHasScreen)
	{
		return;
	}
	const FVector At = Where;
	if (Voice)
	{
		Voice->SetWorldLocation(At);
		Voice->Play(ETNBeachSfx::Slam, 1.f / FMath::Sqrt(SizeK), bHit ? 1.4f : 1.1f);
	}
	TNBeachKit::BurstAt(SandPuff, At + FVector(0.0, 0.0, 60.0), FVector::UpVector, 16);
	TNBeachKit::BurstAt(SandGrains, At + FVector(0.0, 0.0, 40.0), FVector::UpVector, 30);
	UTN_BeachCameraShake::Kick(this, At, bHit ? 0.8f : 0.55f, 1500.f, 6500.f);
}

void ATN_BeachGiantCrab::VisualTick(float DeltaSeconds)
{
	VisualClock += DeltaSeconds;
	FVector Delta = ShownLoc - LastShown;
	Delta.Z = 0.0;
	LastShown = ShownLoc;
	const float Step = static_cast<float>(Delta.Size());
	const float Speed = Step / FMath::Max(DeltaSeconds, 1e-3f);
	Moving = FMath::FInterpTo(Moving, FMath::Clamp(Speed / 300.f, 0.f, 1.5f), DeltaSeconds, 6.f);
	Gait += Step / (TNBeachCrab::Stride * SizeK);

	FVector View = ShownLoc;
	TNBeachKit::LocalCamera(GetWorld(), View);
	const bool bNear = ViewDistance < GetVisualRange();
	if (bNear)
	{
		PoseCrab(DeltaSeconds);
	}

	using TNBeachCrab::EState;
	const EState State = static_cast<EState>(GetMoverState());
	// Espuma en la boca: más cuanto más enfadado.
	if (Body && Foam.ISM.IsValid())
	{
		const TNBeachMeshes::FCrabRig R = TNBeachMeshes::CrabRig();
		const FTransform BodyXf = Body->GetComponentTransform();
		Foam.Origin = BodyXf.TransformPosition(R.Mouth);
		Foam.Desc.Direction = (BodyXf.GetUnitAxis(EAxis::X) + FVector(0.0, 0.0, 0.6)).GetSafeNormal();
		float Rate = 0.6f;
		switch (State)
		{
		case EState::Chase: Rate = 1.8f; break;
		case EState::WindUp: Rate = 3.f; break;
		case EState::Dazed: Rate = 2.5f; break;
		default: break;
		}
		Foam.RateScale = bNear ? Rate : 0.f;
	}
	TNBeachKit::TickEmitterIfBusy(Foam, DeltaSeconds, View);
	TNBeachKit::TickEmitterIfBusy(SandPuff, DeltaSeconds, View);
	TNBeachKit::TickEmitterIfBusy(SandGrains, DeltaSeconds, View);

	// Sombra de la pinza durante el aviso: crece hasta el radio del golpe.
	if (State == EState::WindUp || State == EState::Slam)
	{
		const float A = State == EState::Slam ? 1.f : TNBeachCrab::Smooth01(GetStateAge() / TNBeachCrab::WindUpTime);
		TNBeachKit::PlaceShadow(ClawShadow, Mover.Aim, TNBeachCrab::HitRadius * SizeK * FMath::Lerp(0.35f, 1.f, A));
	}
	else
	{
		TNBeachKit::PlaceShadow(ClawShadow, FVector::ZeroVector, 0.f);
	}

	// Patitas al andar y chasquidos al agitar la pinza.
	if (Voice && bNear)
	{
		SkitterTimer -= DeltaSeconds * Moving;
		if (SkitterTimer <= 0.f && Moving > 0.2f)
		{
			SkitterTimer = 0.45f;
			Voice->SetRelativeLocation(FVector::ZeroVector);
			Voice->Play(ETNBeachSfx::Skitter, (0.9f + 0.2f * FMath::FRand()) / FMath::Sqrt(SizeK), 0.45f * FMath::Min(1.f, Moving));
		}
		if (State == EState::Idle)
		{
			ClackTimer -= DeltaSeconds;
			if (ClackTimer <= 0.f)
			{
				ClackTimer = 1.1f + FMath::FRand();
				Voice->Play(ETNBeachSfx::Clack, 1.1f / FMath::Sqrt(SizeK), 0.6f);
			}
		}
	}
}

void ATN_BeachGiantCrab::PoseCrab(float DeltaSeconds)
{
	using TNBeachCrab::EState;
	const TNBeachMeshes::FCrabRig R = TNBeachMeshes::CrabRig();
	const EState State = static_cast<EState>(GetMoverState());
	const float T = VisualClock;
	const float Age = GetStateAge();
	const float Phase = Gait * 2.f * PI;
	const bool bDazed = State == EState::Dazed;

	// Cuerpo: bote al andar, respiración y un temblor al levantar la pinza.
	float BodyBob = 6.f * FMath::Sin(T * 1.7f) + 18.f * Moving * FMath::Sin(Phase * 2.f);
	float BodyRoll = 3.f * Moving * FMath::Sin(Phase);
	if (State == EState::WindUp)
	{
		BodyRoll += 2.f * FMath::Sin(T * 43.f);
	}
	if (bDazed)
	{
		BodyRoll += 6.f * FMath::Sin(T * 3.f);
		BodyBob -= 25.f;
	}
	TNBeachKit::Pose(Body, FVector(0.0, 0.0, R.BodyZ + BodyBob), FRotator(0.f, 0.f, BodyRoll));

	// Patas: cuatro por lado desfasadas, las de un lado a contratiempo de las del otro.
	for (int32 k = 0; k < Legs.Num(); ++k)
	{
		const float Side = k < 4 ? -1.f : 1.f;
		const float LegPhase = Phase + (k % 4) * PI * 0.5f + (Side > 0.f ? PI : 0.f);
		const float Swing = 18.f * Moving * FMath::Sin(LegPhase);
		const float Lift = 14.f * Moving * FMath::Max(0.f, FMath::Cos(LegPhase)) + (bDazed ? 10.f * FMath::Sin(T * 9.f + k) : 0.f);
		TNBeachKit::Pose(Legs[k], R.LegPivot[k], FRotator(0.f, R.LegSplay[k] + Swing, -Side * Lift));
	}

	// Ojos: se balancean; en el aviso miran la sombra; aturdido, dan vueltas.
	for (int32 e = 0; e < Eyes.Num(); ++e)
	{
		const float Sign = e == 0 ? -1.f : 1.f;
		float Yaw = 8.f * FMath::Sin(T * 1.1f + e * 2.f);
		float Pitch = 6.f * FMath::Sin(T * 1.3f + e);
		if (State == EState::WindUp || State == EState::Chase)
		{
			Pitch -= 10.f;
			Yaw *= 0.3f;
		}
		if (bDazed)
		{
			Yaw = Sign * T * 540.f;
			Pitch = 15.f * FMath::Sin(T * 5.f);
		}
		const FVector Pivot = e == 0 ? R.EyeL : R.EyeR;
		TNBeachKit::Pose(Eyes[e], Pivot, FRotator(Pitch, Yaw, Sign * 6.f));
	}

	// Pinza grande: en reposo, agitándola, levantada temblando, cayendo, clavada y volviendo.
	float ArmPitch = 8.f + 4.f * Moving * FMath::Sin(Phase);
	float ArmRoll = 0.f;
	float HandPitch = -10.f;
	float FingerOpen = 10.f;
	switch (State)
	{
	case EState::Idle:
		ArmPitch = 35.f + 15.f * FMath::Sin(T * 3.f);
		FingerOpen = 30.f * FMath::Abs(FMath::Sin(T * 5.f));
		break;
	case EState::Chase:
		FingerOpen = 10.f + 25.f * FMath::Abs(FMath::Sin(T * 6.f));
		break;
	case EState::WindUp:
	{
		const float A = FMath::Clamp(Age / TNBeachCrab::WindUpTime, 0.f, 1.f);
		ArmPitch = FMath::Lerp(8.f, TNBeachCrab::RaisePitch, TNBeachCrab::Smooth01(A * 2.2f)) + 3.5f * A * FMath::Sin(T * 47.f);
		ArmRoll = 3.f * A * FMath::Sin(T * 39.f);
		HandPitch = -25.f;
		FingerOpen = 40.f;
		break;
	}
	case EState::Slam:
	{
		const float U = FMath::Clamp(Age / TNBeachCrab::SlamTime, 0.f, 1.f);
		ArmPitch = FMath::Lerp(TNBeachCrab::RaisePitch, static_cast<float>(R.SlamPitch), U * U);
		HandPitch = FMath::Lerp(-25.f, 0.f, U);
		FingerOpen = FMath::Lerp(40.f, 0.f, U);
		break;
	}
	case EState::Recover:
	{
		const float Stuck = 0.7f;
		if (Age < Stuck)
		{
			ArmPitch = static_cast<float>(R.SlamPitch) + 2.f * FMath::Sin(T * 20.f);
			ArmRoll = 4.f * FMath::Sin(T * 17.f);
		}
		else
		{
			ArmPitch = FMath::Lerp(static_cast<float>(R.SlamPitch), 8.f, TNBeachCrab::Smooth01((Age - Stuck) / (TNBeachCrab::RecoverTime - Stuck)));
		}
		HandPitch = 0.f;
		FingerOpen = 5.f;
		break;
	}
	case EState::Dazed:
		ArmPitch = static_cast<float>(R.SlamPitch) * 0.6f + 3.f * FMath::Sin(T * 2.f);
		FingerOpen = 20.f;
		break;
	default:
		break;
	}
	TNBeachKit::Pose(BigArm, R.BigShoulder, FRotator(ArmPitch, static_cast<float>(R.ArmYaw), ArmRoll));
	TNBeachKit::Pose(BigHand, R.BigElbow, FRotator(HandPitch, 0.f, 0.f));
	TNBeachKit::Pose(BigFinger, R.BigKnuckle, FRotator(FingerOpen, 0.f, 0.f));

	// Pinza pequeña: se mueve sola, más nerviosa al perseguir.
	const float SmallRate = State == EState::Chase ? 7.f : 2.3f;
	TNBeachKit::Pose(SmallClaw, R.SmallShoulder, FRotator(10.f + 12.f * FMath::Sin(T * SmallRate), 8.f, 0.f));
}
