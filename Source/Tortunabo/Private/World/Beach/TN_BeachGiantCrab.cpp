#include "World/Beach/TN_BeachGiantCrab.h"
#include "World/Beach/TN_BeachCameraShake.h"
#include "World/Beach/TN_BeachEnemySynth.h"
#include "World/Beach/TN_BeachRaceGenerator.h"
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
	/** Ve a una tortuga a esta distancia del cuerpo (cm, por el tamaño) si está dentro de su cono delantero (±70°). */
	constexpr float DetectRadius = 2200.f;
	constexpr float SightHalfAngle = 70.f;
	/**
	 * Oye a una tortuga a esta distancia (cm, por el tamaño) desde cualquier lado: ×1,3 si corre (más de 6 m/s), ×0,6 si
	 * va agachada, en bola, en panzazo o casi quieta (menos de 0,6 m/s).
	 */
	constexpr float HearRadius = 1000.f;
	constexpr float HearLoud = 1.3f;
	constexpr float HearQuiet = 0.6f;
	constexpr float LoudSpeed = 600.f;
	constexpr float QuietSpeed = 60.f;
	/** La deja si la tortuga se aleja de su sitio más que esto (o 1,4 veces la huella). */
	constexpr float LeashRadius = 3800.f;
	/** Velocidades (cm/s, por el tamaño): paseo, persecución (tortuga: 450 andando, 800 esprintando) y vuelta al recorrido. */
	constexpr float PatrolSpeed = 300.f;
	constexpr float ChaseSpeed = 560.f;
	constexpr float ReturnSpeed = 420.f;
	/** Paradas del recorrido (s): cortas, chasqueando la pinza. Si no llega a un punto en este tiempo, pasa al siguiente. */
	constexpr float StopMin = 0.5f;
	constexpr float StopMax = 0.95f;
	constexpr float LegTimeout = 12.f;
	/** Radio del cuerpo en planta (cm, por el tamaño) para apartarse de los demás y de lo grande del reparto. */
	constexpr float BodyRadius = 420.f;
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
	NetFrequencyNear = 12.f;
	SetNetUpdateFrequency(NetFrequencyNear);
	bThrottleWhenFar = true;
}

float ATN_BeachGiantCrab::GetBodyRadius() const
{
	return TNBeachCrab::BodyRadius * SizeK;
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
	HearRadius = TNBeachCrab::HearRadius * SizeK;
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

void ATN_BeachGiantCrab::BuildRoute()
{
	bRouteBuilt = true;
	Route.Reset();
	RouteStops.Reset();
	const float BodyR = GetBodyRadius();
	const float Yaw0 = ServerRng.FRandRange(0.f, 360.f);
	const FVector Axis = FRotator(0.f, Yaw0, 0.f).Vector();
	const FVector Side = FRotator(0.f, Yaw0 + 90.f, 0.f).Vector();
	const float Kind = ServerRng.FRand();

	// Entre dos rocas (o troncos, maderas, castillos pequeños) que tenga cerca y a lados distintos: se para junto a cada una.
	if (Kind < 0.35f)
	{
		if (const ATN_BeachRaceGenerator* Gen = FindGenerator())
		{
			const FTransform GenXf = Gen->GetActorTransform();
			struct FRockSpot
			{
				FVector Pos;
				float Radius;
			};
			TArray<FRockSpot> Rocks;
			for (const TNBeachLayout::FItem& Item : Gen->GetRoundLayout().Items)
			{
				switch (Item.Element)
				{
				case ETNBeachElement::Rock:
				case ETNBeachElement::RockCluster:
				case ETNBeachElement::MossyLog:
				case ETNBeachElement::Driftwood:
				case ETNBeachElement::OldPlanks:
				case ETNBeachElement::SandCastleSmall:
				case ETNBeachElement::Sandbags:
				case ETNBeachElement::TankTrap:
					break;
				default:
					continue;
				}
				const FVector Pos = GenXf.TransformPosition(FVector(Item.Pos.X, Item.Pos.Y, 0.0));
				if (FVector::Dist2D(Pos, Home) < LeashRadius * 0.9f)
				{
					Rocks.Add({ Pos, static_cast<float>(Item.Radius) });
				}
			}
			// La pareja más separada en ángulo (vistas desde su sitio), si lo está de verdad.
			int32 BestA = INDEX_NONE;
			int32 BestB = INDEX_NONE;
			double BestDot = -0.2;
			for (int32 a = 0; a < Rocks.Num(); ++a)
			{
				const FVector DirA = (Rocks[a].Pos - Home).GetSafeNormal2D();
				for (int32 b = a + 1; b < Rocks.Num(); ++b)
				{
					const double Dot = FVector::DotProduct(DirA, (Rocks[b].Pos - Home).GetSafeNormal2D());
					if (Dot < BestDot)
					{
						BestDot = Dot;
						BestA = a;
						BestB = b;
					}
				}
			}
			if (BestA != INDEX_NONE)
			{
				for (const int32 R : { BestA, BestB })
				{
					const FVector ToHome = (Home - Rocks[R].Pos).GetSafeNormal2D();
					Route.Add(Rocks[R].Pos + ToHome * (Rocks[R].Radius * 0.85f + BodyR + 250.f));
					RouteStops.Add(1);
				}
				Route.Insert(Home, 1);
				RouteStops.Insert(static_cast<uint8>(0), 1);
				bRouteLoops = false;
			}
		}
	}
	// Un óvalo alrededor de su sitio (dos paradas por vuelta).
	if (Route.Num() == 0 && Kind < 0.7f)
	{
		const float A = PatrolRadius * 0.9f;
		const float B = A * ServerRng.FRandRange(0.45f, 0.7f);
		constexpr int32 N = 8;
		for (int32 k = 0; k < N; ++k)
		{
			const float Ang = 2.f * PI * k / N;
			Route.Add(Home + Axis * (FMath::Cos(Ang) * A) + Side * (FMath::Sin(Ang) * B));
			RouteStops.Add(k % 4 == 0 ? 1 : 0);
		}
		bRouteLoops = true;
	}
	// Ida y vuelta de lado por una recta que pasa por su sitio (se para en los extremos).
	if (Route.Num() == 0)
	{
		const float L = PatrolRadius * 0.9f;
		Route.Add(Home - Axis * L);
		Route.Add(Home);
		Route.Add(Home + Axis * L);
		RouteStops.Add(1);
		RouteStops.Add(0);
		RouteStops.Add(1);
		bRouteLoops = false;
	}
	// Ningún punto dentro de lo grande del reparto: se acerca a su sitio hasta que quede libre.
	for (FVector& P : Route)
	{
		for (int32 k = 0; k < 5 && IsInsideObstacle(P, BodyR); ++k)
		{
			P = FMath::Lerp(P, Home, 0.3);
		}
		P.Z = Home.Z;
	}
	RouteIndex = ServerRng.RandRange(0, Route.Num() - 1);
	RouteStep = ServerRng.FRand() < 0.5f ? 1 : -1;
}

void ATN_BeachGiantCrab::AdvanceRoute()
{
	const int32 Num = Route.Num();
	if (Num < 2)
	{
		return;
	}
	if (bRouteLoops)
	{
		RouteIndex = (RouteIndex + RouteStep + Num) % Num;
		return;
	}
	if (RouteIndex + RouteStep >= Num || RouteIndex + RouteStep < 0)
	{
		RouteStep = -RouteStep;
	}
	RouteIndex = FMath::Clamp(RouteIndex + RouteStep, 0, Num - 1);
}

int32 ATN_BeachGiantCrab::NearestRoutePoint() const
{
	int32 Best = 0;
	double BestSq = 1.0e18;
	for (int32 i = 0; i < Route.Num(); ++i)
	{
		const double DistSq = FVector::DistSquared2D(Route[i], SimLoc);
		if (DistSq < BestSq)
		{
			BestSq = DistSq;
			Best = i;
		}
	}
	return Best;
}

ATortugaCharacter* ATN_BeachGiantCrab::Perceive() const
{
	TArray<ATortugaCharacter*> Turtles;
	GatherTurtles(this, Turtles);
	const FVector Facing = FRotator(0.f, SimYaw, 0.f).Vector();
	const double CosSight = FMath::Cos(FMath::DegreesToRadians(static_cast<double>(TNBeachCrab::SightHalfAngle)));
	ATortugaCharacter* Best = nullptr;
	double BestSq = 1.0e18;
	for (ATortugaCharacter* Turtle : Turtles)
	{
		if (!IsTargetable(Turtle))
		{
			continue;
		}
		const FVector At = Turtle->GetActorLocation();
		if (FVector::Dist2D(At, Home) > LeashRadius)
		{
			continue;
		}
		FVector To = At - SimLoc;
		To.Z = 0.0;
		const double DistSq = To.SizeSquared();
		if (DistSq >= BestSq)
		{
			continue;
		}
		const double Dist = FMath::Sqrt(DistSq);
		// De frente la ve lejos; alrededor la oye: más si corre, menos si va agachada, en bola o casi quieta.
		bool bSensed = Dist < DetectRadius && (Dist < 1.0 || FVector::DotProduct(To / Dist, Facing) >= CosSight);
		if (!bSensed)
		{
			const float Speed = static_cast<float>(Turtle->GetVelocity().Size2D());
			float Hear = HearRadius;
			if (Turtle->IsInShell() || Turtle->IsBellyPoseActive() || Turtle->bIsCrouched || Speed < TNBeachCrab::QuietSpeed)
			{
				Hear *= TNBeachCrab::HearQuiet;
			}
			else if (Speed > TNBeachCrab::LoudSpeed)
			{
				Hear *= TNBeachCrab::HearLoud;
			}
			bSensed = Dist < Hear;
		}
		if (bSensed)
		{
			BestSq = DistSq;
			Best = Turtle;
		}
	}
	return Best;
}

FVector ATN_BeachGiantCrab::StepToward(const FVector& Goal, float Speed, float DeltaSeconds, FVector& OutDir)
{
	FVector Flat = Goal - SimLoc;
	Flat.Z = 0.0;
	const double Dist = Flat.Size();
	OutDir = Dist > 1.0 ? Flat / Dist : FVector::ZeroVector;
	FVector Next = SimLoc + OutDir * FMath::Min(Dist, static_cast<double>(Speed * DeltaSeconds));
	// Sin meterse en otro enemigo ni en lo grande del reparto (lo rodea).
	Next = ResolveStep(Next, GetBodyRadius(), true);
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
		if (GroundHeightAt(Next, Z))
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
			if (!CanBeHit(Turtle))
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
	if (!bRouteBuilt)
	{
		BuildRoute();
	}

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
			if (ATortugaCharacter* Sensed = Perceive())
			{
				Target = Sensed;
				ServerSetState(ToByte(EState::Chase));
				break;
			}
		}
		if (State == EState::Patrol)
		{
			// Siempre andando su recorrido; en los puntos de parada, un momento chasqueando la pinza.
			PatrolGoal = Route.IsValidIndex(RouteIndex) ? Route[RouteIndex] : Home;
			ServerWalk(PatrolGoal, TNBeachCrab::PatrolSpeed * SizeK, DeltaSeconds, true);
			if (FVector::Dist2D(SimLoc, PatrolGoal) < 150.0 || GetStateAge() > TNBeachCrab::LegTimeout)
			{
				const bool bStop = RouteStops.IsValidIndex(RouteIndex) && RouteStops[RouteIndex] != 0;
				AdvanceRoute();
				if (bStop)
				{
					StateLeft = ServerRng.FRandRange(TNBeachCrab::StopMin, TNBeachCrab::StopMax);
					ServerSetState(ToByte(EState::Idle));
				}
				else
				{
					ServerSetState(ToByte(EState::Patrol));
				}
			}
		}
		else if (StateLeft <= 0.f)
		{
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
			Target = bCanSee ? Perceive() : nullptr;
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
				Next = bCanSee ? Perceive() : nullptr;
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
			if (ATortugaCharacter* Sensed = Perceive())
			{
				Target = Sensed;
				ServerSetState(ToByte(EState::Chase));
				break;
			}
		}
		// Vuelve a su recorrido por el punto más cercano y sigue patrullando desde ahí.
		const int32 Nearest = NearestRoutePoint();
		const FVector Back = Route.IsValidIndex(Nearest) ? Route[Nearest] : Home;
		ServerWalk(Back, TNBeachCrab::ReturnSpeed * SizeK, DeltaSeconds, true);
		if (FVector::Dist2D(SimLoc, Back) < 200.0 || GetStateAge() > TNBeachCrab::LegTimeout * 2.f)
		{
			RouteIndex = Nearest;
			AdvanceRoute();
			ServerSetState(ToByte(EState::Patrol));
		}
		break;
	}
	}

	if (IsDebugDraw())
	{
		UWorld* World = GetWorld();
		DrawDebugCircle(World, SimLoc + FVector(0.0, 0.0, 40.0), DetectRadius, 48, FColor::Yellow, false, -1.f, 0, 10.f, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), false);
		DrawDebugCircle(World, Home + FVector(0.0, 0.0, 40.0), LeashRadius, 64, FColor::Orange, false, -1.f, 0, 10.f, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), false);
		DrawDebugCircle(World, SimLoc + FVector(0.0, 0.0, 40.0), HearRadius, 32, FColor::Cyan, false, -1.f, 0, 8.f, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), false);
		for (int32 i = 0; i + 1 < Route.Num() + (bRouteLoops ? 1 : 0); ++i)
		{
			const FVector A = Route[i] + FVector(0.0, 0.0, 60.0);
			const FVector B = Route[(i + 1) % Route.Num()] + FVector(0.0, 0.0, 60.0);
			DrawDebugLine(World, A, B, RouteStops.IsValidIndex(i) && RouteStops[i] ? FColor::Green : FColor(0, 120, 0), false, -1.f, 0, 8.f);
		}
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
				// Paradas cortas del recorrido: chasquidos seguidos de la pinza.
				ClackTimer = 0.28f + 0.25f * FMath::FRand();
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
		// Parada del recorrido: pinza en alto chasqueando deprisa.
		ArmPitch = 38.f + 12.f * FMath::Sin(T * 4.f);
		FingerOpen = 32.f * FMath::Abs(FMath::Sin(T * 11.f));
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
