#include "World/Beach/TN_BeachGullZone.h"
#include "World/Beach/TN_BeachCameraShake.h"
#include "World/Beach/TN_BeachEnemySynth.h"
#include "World/Beach/TN_BeachRaceGenerator.h"
#include "World/Beach/TN_BeachStun.h"
#include "TN_BeachEnemyKit.h"
#include "TN_BeachEnemyMeshes.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Player/TN_ShellBody.h"
#include "Player/TN_ShellComponent.h"
#include "Player/TortugaCharacter.h"

namespace TNBeachGull
{
	/** Tiempo entre ataques (s) mientras haya tortugas debajo. */
	constexpr float AttackMin = 3.5f;
	constexpr float AttackMax = 7.f;
	/** Cagada: vuela sobre la tortuga y la suelta; cae en FallTime; fin del ataque. */
	constexpr float DropTime = 1.4f;
	constexpr float FallTime = 1.6f;
	constexpr float PoopEnd = 5.f;
	/** Altura (cm) a la que la suelta, radio de la mancha (por el tamaño), aturdimiento y adelanto al apuntar (s). */
	constexpr float PoopHeight = 4500.f;
	constexpr float SplatRadius = 230.f;
	constexpr float PoopStun = 2.6f;
	constexpr float LeadPoop = 0.35f;
	/** Picado: sube y se coloca, deja de corregir, llega abajo, la sube y fin del ataque (con y sin presa). */
	constexpr float ClimbTime = 0.9f;
	constexpr float LockTime = 1.8f;
	constexpr float StrikeTime = 2.3f;
	constexpr float CarryTime = 2.f;
	constexpr float DiveEndHit = 6.5f;
	constexpr float DiveEndMiss = 5.5f;
	/** De dónde arranca el picado (cm desde la tortuga) y radio en el que la coge (por el tamaño). */
	constexpr float DiveStartDist = 5000.f;
	constexpr float DiveStartHeight = 4800.f;
	constexpr float GrabRadius = 300.f;
	constexpr float LeadDive = 0.25f;
	/** La sube 32 m y la lleva 18 m hacia la salida (-X) antes de soltarla; aturdida 3 s tras caer. */
	constexpr float CarryHeight = 3200.f;
	constexpr float CarryBack = 1800.f;
	constexpr float AfterDropStun = 3.f;
	constexpr float Gravity = 980.f;
	/** Pico por encima del centro de la tortuga mientras la lleva (cm a escala 28). */
	constexpr float BeakOffset = 380.f;

	inline float Smooth01(float X)
	{
		const float C = FMath::Clamp(X, 0.f, 1.f);
		return C * C * (3.f - 2.f * C);
	}

	/** Camino de la tortuga que se lleva: sube con curva y va hacia la salida (Back). */
	inline FVector CarryPath(const FVector& Start, const FVector& Back, float U)
	{
		const float C = FMath::Clamp(U, 0.f, 1.f);
		return Start + Back * (CarryBack * C) + FVector(0.0, 0.0, CarryHeight * Smooth01(C));
	}
}

ATN_BeachGullZone::ATN_BeachGullZone()
{
	bUsesMover = false;
	SetNetUpdateFrequency(10.f);
}

void ATN_BeachGullZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_BeachGullZone, Attack);
}

void ATN_BeachGullZone::ApplySpec()
{
	SizeK = FMath::Clamp(Spec.SizeScale, 0.75f, 1.3f);
	AttackRadius = GetFootprintRadius() + 800.f;
	CircleRadius = FMath::Max(3500.f, GetFootprintRadius() * 1.25f);
	BuildBirds();
}

void ATN_BeachGullZone::BeginPlay()
{
	Super::BeginPlay();
	// Hacia la salida: al revés de hacia donde está el mar según el generador (sin él, -X).
	if (const ATN_BeachRaceGenerator* Generator = ATN_BeachRaceGenerator::Find(this))
	{
		const FVector Sea = Generator->GetSeaDirection().GetSafeNormal2D();
		if (!Sea.IsNearlyZero())
		{
			CourseBack = -Sea;
		}
	}
	if (HasAuthority())
	{
		NextAttackTime = ServerNow(this) + ServerRng.FRandRange(2.f, 5.f);
	}
}

void ATN_BeachGullZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (UStaticMeshComponent* Splat : Splats)
	{
		if (Splat)
		{
			Splat->DestroyComponent();
		}
	}
	Splats.Reset();
	Super::EndPlay(EndPlayReason);
}

// ─────────────────────────────────────────────────────────────────────────────
// Pájaros
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachGullZone::BuildBirds()
{
	if (Birds.Num() > 0)
	{
		return;
	}
	// Parámetros de vuelo en todas las máquinas (los mismos con la misma semilla).
	const uint32 Seed = static_cast<uint32>(Spec.Seed);
	const int32 NumGulls = 3 + (SizeK > 1.05f ? 1 : 0);
	const bool bWithPelican = TNBeachKit::Hash01(Seed * 3u + 1u) < 0.6f;
	const int32 Count = NumGulls + (bWithPelican ? 1 : 0);
	const float Turn = TNBeachKit::Hash01(Seed * 7u + 5u) < 0.5f ? 1.f : -1.f;
	for (int32 b = 0; b < Count; ++b)
	{
		FBird& Bird = Birds.AddDefaulted_GetRef();
		Bird.bPelican = bWithPelican && b == Count - 1;
		Bird.Radius = CircleRadius * FMath::Lerp(0.85f, 1.25f, TNBeachKit::Hash01(Seed + b * 17u));
		Bird.Height = FMath::Lerp(5500.f, 8000.f, TNBeachKit::Hash01(Seed + b * 31u)) * (Bird.bPelican ? 0.85f : 1.f);
		Bird.Phase = 2.f * PI * b / Count + 0.6f * TNBeachKit::Hash01(Seed + b * 13u);
		Bird.AngSpeed = Turn * (0.14f + 0.08f * TNBeachKit::Hash01(Seed + b * 23u)) * (Bird.bPelican ? 0.7f : 1.f);
		Bird.Scale = (Bird.bPelican ? 24.f : 28.f) * FMath::Sqrt(SizeK);
		Bird.Span = (Bird.bPelican ? 170.f : 90.f) * Bird.Scale;
		Bird.SquawkTimer = 2.f + 6.f * TNBeachKit::Hash01(Seed + b * 41u);
	}
	if (!bHasScreen)
	{
		return;
	}

	// Mallas de la fauna (gaviota y pelícano), compartidas por pieza.
	TArray<TNFauna::FTNFaunaPart> GullParts;
	TArray<TNFauna::FTNFaunaPart> PelicanParts;
	TNFauna::FTNFaunaRig GullRig;
	TNFauna::FTNFaunaRig PelicanRig;
	TNFauna::TNFaunaBuildSpecies(TNFauna::ETNFaunaSpecies::Gull, GullParts, GullRig);
	if (bWithPelican)
	{
		TNFauna::TNFaunaBuildSpecies(TNFauna::ETNFaunaSpecies::Pelican, PelicanParts, PelicanRig);
	}
	for (int32 b = 0; b < Birds.Num(); ++b)
	{
		FBird& Bird = Birds[b];
		const TArray<TNFauna::FTNFaunaPart>& Parts = Bird.bPelican ? PelicanParts : GullParts;
		USceneComponent* BirdRoot = NewObject<USceneComponent>(this, NAME_None, RF_Transient);
		BirdRoot->SetupAttachment(GetRootComponent());
		BirdRoot->SetAbsolute(true, true, true);
		BirdRoot->RegisterComponent();
		BirdRoots.Add(BirdRoot);
		Bird.FirstPart = BirdParts.Num();
		UStaticMeshComponent* BodyComp = nullptr;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			for (int32 i = 0; i < Parts.Num(); ++i)
			{
				const TNFauna::FTNFaunaPart& Part = Parts[i];
				const bool bIsBody = Part.Bone == TNFauna::ETNFaunaBone::Body;
				if ((Pass == 0) != bIsBody)
				{
					continue;
				}
				const TNProcMesh::FTNProcMeshBuffers& Buffers = Part.Mesh;
				UStaticMesh* Mesh = TNBeachKit::CachedMesh(FString::Printf(TEXT("Beach.%s.%d"), Bird.bPelican ? TEXT("Pelican") : TEXT("Gull"), i),
					[&Buffers](TNProcMesh::FTNProcMeshBuffers& M) { M = Buffers; });
				USceneComponent* Parent = bIsBody ? BirdRoot : static_cast<USceneComponent*>(BodyComp);
				UStaticMeshComponent* Comp = TNBeachKit::AddPart(this, Parent ? Parent : BirdRoot, Mesh, Part.Pivot, false);
				if (bIsBody && !BodyComp)
				{
					BodyComp = Comp;
				}
				const int32 Index = BirdParts.Add(Comp);
				PartPivots.Add(Part.Pivot);
				switch (Part.Bone)
				{
				case TNFauna::ETNFaunaBone::WingL: Bird.WingL = Index; break;
				case TNFauna::ETNFaunaBone::WingR: Bird.WingR = Index; break;
				case TNFauna::ETNFaunaBone::LegBL: Bird.LegL = Index; break;
				case TNFauna::ETNFaunaBone::LegBR: Bird.LegR = Index; break;
				case TNFauna::ETNFaunaBone::Head: Bird.Head = Index; break;
				default: break;
				}
			}
		}
		Bird.NumParts = BirdParts.Num() - Bird.FirstPart;
		UStaticMeshComponent* Shadow = TNBeachKit::AddShadow(this, 0.38f);
		TNBeachKit::PlaceShadow(Shadow, FVector::ZeroVector, 0.f);
		Shadows.Add(Shadow);
		Bird.ShadowZ = static_cast<float>(GetActorLocation().Z);
		Bird.ShadowTimer = 0.05f * b;
	}

	UStaticMesh* DropMesh = TNBeachKit::CachedMesh(TEXT("Beach.Dropping"), [](TNProcMesh::FTNProcMeshBuffers& M) { TNBeachMeshes::BuildDropping(M); });
	Dropping = TNBeachKit::AddPart(this, GetRootComponent(), DropMesh, FVector::ZeroVector, false);
	if (Dropping)
	{
		Dropping->SetAbsolute(true, true, true);
		Dropping->SetVisibility(false);
	}
	DropShadow = TNBeachKit::AddShadow(this, 0.55f);
	TNBeachKit::PlaceShadow(DropShadow, FVector::ZeroVector, 0.f);

	using TNAmbientFX::EShape;
	TNAmbientFX::FEmitterDesc DropDesc = TNBeachKit::MakeDesc(EShape::Drop, FLinearColor(0.97f, 0.97f, 0.94f), false, 1.f, 30, 0.f, 900.f, -1800.f, 0.5f, 1.f, 45.f, 35.f);
	DropDesc.Spread = 0.9f;
	DropDesc.SpawnRadius = 120.f;
	TNBeachKit::InitEmitter(Droplets, this, DropDesc, Seed + 51u);
	TNAmbientFX::FEmitterDesc FeatherDesc = TNBeachKit::MakeDesc(EShape::Flake, FLinearColor(0.95f, 0.95f, 0.93f), false, 1.f, 24, 0.f, 500.f, -120.f, 1.5f, 3.f, 90.f, 70.f);
	FeatherDesc.Spread = 1.f;
	FeatherDesc.SpawnRadius = 300.f;
	TNBeachKit::InitEmitter(Feathers, this, FeatherDesc, Seed + 52u);
	GetVoice(GetRootComponent(), 2500.f, 16000.f);
}

FVector ATN_BeachGullZone::CirclePos(const FBird& Bird, double Now) const
{
	const double A = Bird.Phase + Bird.AngSpeed * Now;
	const FVector Center = GetActorLocation();
	return Center + FVector(FMath::Cos(A) * Bird.Radius, FMath::Sin(A) * Bird.Radius, Bird.Height + 300.0 * FMath::Sin(0.37 * Now + Bird.Phase * 3.0));
}

FVector ATN_BeachGullZone::AttackPos(const FBird& Bird, double Now)
{
	using namespace TNBeachGull;
	const float Tau = static_cast<float>(Now - static_cast<double>(Attack.StartTime));
	const FVector Circle = CirclePos(Bird, Now);
	const FVector From = CirclePos(Bird, Attack.StartTime);
	const ATortugaCharacter* Victim = Attack.Victim;
	const FVector VictimLoc = Victim ? Victim->GetActorLocation() : FVector(Attack.Aim);
	const FVector Target = Attack.bLocked ? FVector(Attack.Aim) : VictimLoc;
	if (Attack.Kind == 1)
	{
		// Cagada: arco hasta encima de la tortuga, pasa de largo subiendo y vuelve a su vuelta.
		const FVector Over = Target + FVector(0.0, 0.0, PoopHeight);
		if (Tau < DropTime)
		{
			const float A = Smooth01(Tau / DropTime);
			return FMath::Lerp(From, Over, static_cast<double>(A)) + FVector(0.0, 0.0, 800.0 * FMath::Sin(PI * A));
		}
		FVector Dir = Over - From;
		Dir.Z = 0.0;
		Dir = Dir.IsNearlyZero() ? FVector::ForwardVector : Dir.GetSafeNormal();
		const float Past = FMath::Min(Tau, DropTime + FallTime) - DropTime;
		const FVector Beyond = Over + Dir * (Past * 1800.0) + FVector(0.0, 0.0, Past * 400.0);
		if (Tau < DropTime + FallTime)
		{
			return Beyond;
		}
		const float B = Smooth01((Tau - DropTime - FallTime) / (PoopEnd - DropTime - FallTime));
		return FMath::Lerp(Beyond, Circle, static_cast<double>(B));
	}

	// Picado: se coloca lejos y alto, se lanza acelerando hacia la sombra y, si la coge, se la lleva.
	FVector Away = From - Target;
	Away.Z = 0.0;
	Away = Away.IsNearlyZero() ? FVector::ForwardVector : Away.GetSafeNormal();
	const FVector DiveStart = Target + Away * DiveStartDist + FVector(0.0, 0.0, DiveStartHeight);
	const FVector Strike = Target + FVector(0.0, 0.0, 250.0 * Bird.Scale / 28.0);
	if (Tau < ClimbTime)
	{
		return FMath::Lerp(From, DiveStart, static_cast<double>(Smooth01(Tau / ClimbTime)));
	}
	if (Tau < StrikeTime)
	{
		const float U = (Tau - ClimbTime) / (StrikeTime - ClimbTime);
		return FMath::Lerp(DiveStart, Strike, static_cast<double>(U * U));
	}
	const FVector Beak(0.0, 0.0, BeakOffset * Bird.Scale / 28.0);
	if (Attack.Result == 1)
	{
		if (Tau < StrikeTime + CarryTime)
		{
			return VictimLoc + Beak;
		}
		if (!bHasReleasePos)
		{
			ReleasePos = VictimLoc + Beak;
			bHasReleasePos = true;
		}
		const float B = Smooth01((Tau - StrikeTime - CarryTime) / (DiveEndHit - StrikeTime - CarryTime));
		return FMath::Lerp(ReleasePos + FVector(0.0, 0.0, 1500.0 * B), Circle, static_cast<double>(B));
	}
	// Fallo (o aún sin saberlo): remonta de largo y vuelve.
	FVector Fwd = Target - DiveStart;
	Fwd.Z = 0.0;
	Fwd = Fwd.IsNearlyZero() ? FVector::ForwardVector : Fwd.GetSafeNormal();
	const float K = FMath::Min(Tau - StrikeTime, 0.9f);
	const FVector Pull = Strike + Fwd * (K * 3000.0) + FVector(0.0, 0.0, K * K * 3500.0);
	if (Tau < StrikeTime + 0.9f)
	{
		return Pull;
	}
	const float B = Smooth01((Tau - StrikeTime - 0.9f) / (DiveEndMiss - StrikeTime - 0.9f));
	return FMath::Lerp(Pull, Circle, static_cast<double>(B));
}

float ATN_BeachGullZone::GroundAt(const FVector& Where) const
{
	float Z = static_cast<float>(GetActorLocation().Z);
	TraceGround(this, FVector(Where.X, Where.Y, FMath::Max(Where.Z, GetActorLocation().Z)), Z, nullptr, 1500.f, 14000.f);
	return Z;
}

// ─────────────────────────────────────────────────────────────────────────────
// Servidor
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachGullZone::DebugAttackNow(int32 InKind)
{
	if (!HasAuthority() || Birds.Num() == 0)
	{
		return;
	}
	TArray<ATortugaCharacter*> Turtles;
	GatherTurtles(this, Turtles);
	ATortugaCharacter* Best = nullptr;
	double BestSq = 1e30;
	for (ATortugaCharacter* Turtle : Turtles)
	{
		const double DistSq = FVector::DistSquared2D(Turtle->GetActorLocation(), GetActorLocation());
		if (DistSq < BestSq)
		{
			BestSq = DistSq;
			Best = Turtle;
		}
	}
	if (!Best)
	{
		return;
	}
	const int32 BirdIndex = ServerRng.RandRange(0, Birds.Num() - 1);
	const uint8 Kind = InKind == 1 || InKind == 2 ? static_cast<uint8>(InKind) : static_cast<uint8>(ServerRng.FRand() < 0.5f ? 1 : 2);
	StartAttack(Best, Kind, BirdIndex);
}

void ATN_BeachGullZone::StartAttack(ATortugaCharacter* Victim, uint8 InKind, int32 BirdIndex)
{
	Attack.Victim = Victim;
	Attack.Aim = Victim ? Victim->GetActorLocation() : GetActorLocation();
	Attack.StartTime = static_cast<float>(ServerNow(this));
	Attack.Kind = InKind;
	Attack.Bird = static_cast<uint8>(FMath::Clamp(BirdIndex, 0, 255));
	Attack.Result = 0;
	Attack.bLocked = 0;
	++Attack.Serial;
	bReleased = false;
	ForceNetUpdate();
	OnAttackChanged();
}

void ATN_BeachGullZone::EndAttack(double Now)
{
	Attack.Kind = 0;
	Attack.Victim = nullptr;
	NextAttackTime = Now + ServerRng.FRandRange(TNBeachGull::AttackMin, TNBeachGull::AttackMax);
	ForceNetUpdate();
	OnAttackChanged();
}

void ATN_BeachGullZone::ServerTick(float DeltaSeconds)
{
	const double Now = ServerNow(this);
	if (Attack.Kind != 0)
	{
		const float Tau = static_cast<float>(Now - static_cast<double>(Attack.StartTime));
		if (Attack.Kind == 1)
		{
			ServerPoop(Tau);
		}
		else
		{
			ServerDive(Tau, DeltaSeconds);
		}
		if (Attack.Kind != 0 && Tau > 12.f)
		{
			EndAttack(Now);
		}
		return;
	}
	if (Now < NextAttackTime || !IsRaceLive(this) || Birds.Num() == 0)
	{
		return;
	}
	// A por una de las tortugas que están debajo (sin sombrilla y sin aturdir).
	TArray<ATortugaCharacter*> Turtles;
	GatherTurtles(this, Turtles);
	TArray<ATortugaCharacter*> Candidates;
	for (ATortugaCharacter* Turtle : Turtles)
	{
		if (IsTargetable(Turtle) && !Turtle->HasUmbrellaProtection() && FVector::Dist2D(Turtle->GetActorLocation(), GetActorLocation()) < AttackRadius)
		{
			Candidates.Add(Turtle);
		}
	}
	if (Candidates.Num() == 0)
	{
		NextAttackTime = Now + 0.5;
		return;
	}
	ATortugaCharacter* Victim = Candidates[ServerRng.RandRange(0, Candidates.Num() - 1)];
	const int32 BirdIndex = ServerRng.RandRange(0, Birds.Num() - 1);
	// El pelícano siempre coge (picado); las gaviotas, la mitad de las veces cagan.
	const uint8 Kind = Birds[BirdIndex].bPelican ? 2 : static_cast<uint8>(ServerRng.FRand() < 0.5f ? 1 : 2);
	StartAttack(Victim, Kind, BirdIndex);

	if (IsDebugDraw())
	{
		DrawDebugCircle(GetWorld(), GetActorLocation() + FVector(0.0, 0.0, 30.0), AttackRadius, 48, FColor::Yellow, false, 2.f, 0, 10.f, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), false);
	}
}

void ATN_BeachGullZone::ServerPoop(float Tau)
{
	using namespace TNBeachGull;
	if (!Attack.bLocked && Tau >= DropTime)
	{
		// Se fija dónde cae (con adelanto). La traza desde arriba da en el techo si lo hay: la sombra cae encima.
		const ATortugaCharacter* Victim = Attack.Victim;
		FVector Point = FVector(Attack.Aim);
		if (Victim)
		{
			const FVector Vel = Victim->GetVelocity();
			Point = Victim->GetActorLocation() + FVector(Vel.X, Vel.Y, 0.0) * LeadPoop;
		}
		float Z = static_cast<float>(Point.Z);
		if (TraceGround(this, Point, Z, nullptr, 3000.f, 3000.f))
		{
			Point.Z = Z;
		}
		Attack.Aim = Point;
		Attack.bLocked = 1;
		ForceNetUpdate();
	}
	if (Attack.Result == 0 && Tau >= DropTime + FallTime)
	{
		const FVector Impact = Attack.Aim;
		TArray<ATortugaCharacter*> Hit;
		if (IsRaceLive(this))
		{
			TArray<ATortugaCharacter*> Turtles;
			GatherTurtles(this, Turtles);
			for (ATortugaCharacter* Turtle : Turtles)
			{
				if (!IsValid(Turtle) || TNBeach::IsTurtleStunned(Turtle) || Turtle->HasUmbrellaProtection())
				{
					continue;
				}
				const FVector At = Turtle->GetActorLocation();
				// A cubierto (bajo una sombrilla, en el castillo) la mancha cae encima, no en ella.
				if (FVector::Dist2D(At, Impact) > SplatRadius * SizeK + 45.f || FMath::Abs(At.Z - Impact.Z) > 300.0)
				{
					continue;
				}
				StunTurtle(Turtle, PoopStun, FVector::ZeroVector);
				Hit.Add(Turtle);
			}
		}
		Attack.Result = Hit.Num() > 0 ? 1 : 2;
		ForceNetUpdate();
		MulticastSplat(Impact, Hit);
		OnAttackChanged();
	}
	if (Tau >= PoopEnd)
	{
		EndAttack(ServerNow(this));
	}
}

void ATN_BeachGullZone::ServerDive(float Tau, float DeltaSeconds)
{
	using namespace TNBeachGull;
	if (!Attack.bLocked && Tau >= LockTime)
	{
		const ATortugaCharacter* Victim = Attack.Victim;
		FVector Point = FVector(Attack.Aim);
		if (Victim)
		{
			const FVector Vel = Victim->GetVelocity();
			Point = Victim->GetActorLocation() + FVector(Vel.X, Vel.Y, 0.0) * LeadDive;
		}
		float Z = static_cast<float>(Point.Z);
		if (TraceGround(this, Point, Z, nullptr, 400.f, 3000.f))
		{
			Point.Z = Z;
		}
		Attack.Aim = Point;
		Attack.bLocked = 1;
		// Con algo encima (sombrilla, techo) no puede bajar: fallará.
		FHitResult RoofHit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BeachGullRoof), false);
		if (GetWorld()->LineTraceSingleByObjectType(RoofHit, Point + FVector(0.0, 0.0, 3000.0), Point + FVector(0.0, 0.0, 250.0),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			Attack.Result = 2;
			OnAttackChanged();
		}
		ForceNetUpdate();
	}
	if (Attack.Result == 0 && Tau >= StrikeTime)
	{
		// La coge si está bajo el pico y no está en pleno panzazo (el panzazo la esquiva).
		ATortugaCharacter* Caught = nullptr;
		if (IsRaceLive(this))
		{
			const FVector Point = Attack.Aim;
			float Best = GrabRadius * SizeK + 45.f;
			TArray<ATortugaCharacter*> Turtles;
			GatherTurtles(this, Turtles);
			for (ATortugaCharacter* Turtle : Turtles)
			{
				if (!IsTargetable(Turtle) || Turtle->HasUmbrellaProtection() || Turtle->IsBellyPoseActive())
				{
					continue;
				}
				const FVector At = Turtle->GetActorLocation();
				const float Dist = static_cast<float>(FVector::Dist2D(At, Point));
				if (Dist < Best && FMath::Abs(At.Z - Point.Z) < 350.0)
				{
					Best = Dist;
					Caught = Turtle;
				}
			}
		}
		if (Caught)
		{
			Attack.Victim = Caught;
			Attack.Result = 1;
			CarryStart = Caught->GetActorLocation();
			bReleased = false;
			StunTurtle(Caught, CarryTime + 2.6f + AfterDropStun, FVector(0.0, 0.0, 1500.0) + CourseBack * 300.0);
			IgnoreTurtle(Caught, 8.f);
		}
		else
		{
			Attack.Result = 2;
		}
		ForceNetUpdate();
		OnAttackChanged();
	}
	if (Attack.Result == 1 && !bReleased)
	{
		ATortugaCharacter* Carried = Attack.Victim;
		const float U = (Tau - StrikeTime) / CarryTime;
		if (!IsValid(Carried))
		{
			bReleased = true;
		}
		else if (U < 1.f)
		{
			const float Dt = FMath::Max(DeltaSeconds, 1e-3f);
			const FVector OnPath = CarryPath(CarryStart, CourseBack, U);
			const FVector Ahead = CarryPath(CarryStart, CourseBack, U + Dt / CarryTime);
			DriveCarried(OnPath, (Ahead - OnPath) / Dt);
		}
		else
		{
			// La suelta desde arriba: cae en bola y sigue aturdida un rato al llegar al suelo.
			bReleased = true;
			const float FallSeconds = FMath::Sqrt(2.f * CarryHeight / Gravity);
			StunTurtle(Carried, FallSeconds + AfterDropStun, FVector::ZeroVector);
			ReleaseCarried();
		}
	}
	if (Attack.Result != 0 && Tau >= (Attack.Result == 1 ? DiveEndHit : DiveEndMiss))
	{
		EndAttack(ServerNow(this));
	}
}

void ATN_BeachGullZone::DriveCarried(const FVector& Target, const FVector& TargetVel)
{
	ATortugaCharacter* Carried = Attack.Victim;
	UTN_ShellComponent* Shell = Carried ? Carried->FindComponentByClass<UTN_ShellComponent>() : nullptr;
	ATN_ShellBody* ShellBox = Shell ? Shell->GetBody() : nullptr;
	UBoxComponent* Box = ShellBox ? ShellBox->GetBox() : nullptr;
	if (!Box || !Box->IsSimulatingPhysics())
	{
		// Sin caja física: el lanzamiento del aturdimiento ya la ha subido.
		return;
	}
	const FVector Pos = Box->GetComponentLocation();
	const FVector Vel = (TargetVel + (Target - Pos) * 6.0).GetClampedToMaxSize(4000.0);
	Box->SetPhysicsLinearVelocity(Vel);
}

void ATN_BeachGullZone::ReleaseCarried()
{
	ATortugaCharacter* Carried = Attack.Victim;
	UTN_ShellComponent* Shell = Carried ? Carried->FindComponentByClass<UTN_ShellComponent>() : nullptr;
	ATN_ShellBody* ShellBox = Shell ? Shell->GetBody() : nullptr;
	UBoxComponent* Box = ShellBox ? ShellBox->GetBox() : nullptr;
	if (Box && Box->IsSimulatingPhysics())
	{
		Box->SetPhysicsLinearVelocity(CourseBack * 250.0);
		Box->SetPhysicsAngularVelocityInDegrees(FVector(120.0, 200.0, 60.0));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Efectos
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachGullZone::OnRep_Attack()
{
	OnAttackChanged();
}

void ATN_BeachGullZone::OnAttackChanged()
{
	if (!bHasScreen)
	{
		SeenSerial = Attack.Serial;
		SeenResult = Attack.Result;
		return;
	}
	const bool bValidBird = Birds.IsValidIndex(Attack.Bird);
	const float Pitch = bValidBird && Birds[Attack.Bird].bPelican ? 0.55f : 1.f;
	if (Attack.Serial != SeenSerial)
	{
		SeenSerial = Attack.Serial;
		SeenResult = 0;
		bSwoopPlayed = false;
		bHasReleasePos = false;
		if (Attack.Kind != 0 && Voice && bValidBird)
		{
			Voice->SetWorldLocation(Birds[Attack.Bird].Pos);
			Voice->Play(ETNBeachSfx::Squawk, Pitch, 1.1f);
		}
	}
	if (Attack.Result != SeenResult)
	{
		SeenResult = Attack.Result;
		if (Attack.Kind == 2 && Voice && bValidBird)
		{
			Voice->SetWorldLocation(Birds[Attack.Bird].Pos);
			Voice->Play(ETNBeachSfx::Squawk, Pitch * (Attack.Result == 1 ? 1.15f : 0.85f), 1.2f);
			if (Attack.Result == 1 && Attack.Victim)
			{
				TNBeachKit::BurstAt(Feathers, Attack.Victim->GetActorLocation() + FVector(0.0, 0.0, 200.0), FVector::UpVector, 16);
				UTN_BeachCameraShake::Kick(this, Attack.Victim->GetActorLocation(), 0.5f, 400.f, 2500.f);
			}
		}
	}
}

void ATN_BeachGullZone::MulticastSplat_Implementation(FVector_NetQuantize Where, const TArray<ATortugaCharacter*>& Hit)
{
	if (!bHasScreen)
	{
		return;
	}
	const FVector At = Where;
	if (Voice)
	{
		Voice->SetWorldLocation(At);
		Voice->Play(ETNBeachSfx::Splat, 1.f, 1.2f);
	}
	TNBeachKit::BurstAt(Droplets, At + FVector(0.0, 0.0, 40.0), FVector::UpVector, 22);
	SpawnSplat(At, nullptr, TNBeachGull::SplatRadius * SizeK / 100.f, 9.f);
	for (ATortugaCharacter* Turtle : Hit)
	{
		if (Turtle && Turtle->GetRootComponent())
		{
			SpawnSplat(FVector::ZeroVector, Turtle->GetRootComponent(), 1.f, TNBeachGull::PoopStun + 3.f);
		}
	}
}

void ATN_BeachGullZone::SpawnSplat(const FVector& Where, USceneComponent* InParent, float InScale, float Life)
{
	UStaticMesh* Mesh = nullptr;
	if (InParent)
	{
		Mesh = TNBeachKit::CachedMesh(TEXT("Beach.ShellSplat"), [](TNProcMesh::FTNProcMeshBuffers& M) { TNBeachMeshes::BuildShellSplat(M); });
	}
	else
	{
		Mesh = TNBeachKit::CachedMesh(TEXT("Beach.Splat"), [](TNProcMesh::FTNProcMeshBuffers& M) { TNBeachMeshes::BuildSplat(M, 77u); });
	}
	UStaticMeshComponent* Comp = TNBeachKit::AddPart(this, InParent ? InParent : GetRootComponent(), Mesh, FVector::ZeroVector, false);
	if (!Comp)
	{
		return;
	}
	const float Yaw = 360.f * TNBeachKit::Hash01(static_cast<uint32>(Clock * 1000.f) + static_cast<uint32>(Splats.Num()) * 7u);
	if (InParent)
	{
		Comp->SetRelativeLocationAndRotation(FVector(0.0, 0.0, 55.0), FRotator(0.f, Yaw, 0.f));
		Comp->SetRelativeScale3D(FVector(InScale));
	}
	else
	{
		Comp->SetAbsolute(true, true, true);
		Comp->SetWorldTransform(FTransform(FRotator(0.f, Yaw, 0.f), Where + FVector(0.0, 0.0, 4.0), FVector(InScale)));
	}
	Splats.Add(Comp);
	SplatBorn.Add(Clock);
	SplatLife.Add(Life);
	SplatScale.Add(InScale);
}

// ─────────────────────────────────────────────────────────────────────────────
// Visual
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachGullZone::PoseBird(int32 Index, float DeltaSeconds, bool bAttacking, float Tau)
{
	using namespace TNBeachGull;
	const FBird& Bird = Birds[Index];
	const float Rate = Bird.bPelican ? 0.7f : 1.f;
	// Planea con rachas de aleteo; en el picado, alas recogidas; llevándola, aleteo fuerte.
	float Flap = 6.f + 3.f * FMath::Sin(Clock * 1.3f + Bird.Phase);
	const float Cycle = FMath::Fmod(Clock * 0.25f + Bird.Phase, 1.f);
	if (Cycle < 0.3f)
	{
		Flap = 35.f * FMath::Sin(Clock * 2.f * PI * 2.4f * Rate);
	}
	float Sweep = 0.f;
	float Tuck = -70.f;
	float HeadPitch = 0.f;
	if (bAttacking && Attack.Kind == 2)
	{
		if (Tau >= ClimbTime && Tau < StrikeTime)
		{
			Sweep = 55.f;
			Flap = 8.f;
			HeadPitch = -20.f;
			Tuck = -85.f;
		}
		else if (Tau >= StrikeTime && Attack.Result == 1 && Tau < StrikeTime + CarryTime)
		{
			Flap = 45.f * FMath::Sin(Clock * 2.f * PI * 3.2f * Rate);
			Tuck = 20.f;
			HeadPitch = -35.f;
		}
		else if (Tau >= StrikeTime)
		{
			Flap = 40.f * FMath::Sin(Clock * 2.f * PI * 3.f * Rate);
		}
	}
	else if (bAttacking && Attack.Kind == 1 && Tau < DropTime + 0.4f)
	{
		HeadPitch = -25.f;
	}
	auto PosePart = [this](int32 PartIndex, const FRotator& Rot)
	{
		if (BirdParts.IsValidIndex(PartIndex) && PartPivots.IsValidIndex(PartIndex))
		{
			TNBeachKit::Pose(BirdParts[PartIndex], PartPivots[PartIndex], Rot);
		}
	};
	PosePart(Bird.WingL, FRotator(0.f, -Sweep, Flap));
	PosePart(Bird.WingR, FRotator(0.f, Sweep, -Flap));
	PosePart(Bird.LegL, FRotator(Tuck, 0.f, 0.f));
	PosePart(Bird.LegR, FRotator(Tuck, 0.f, 0.f));
	PosePart(Bird.Head, FRotator(HeadPitch, 6.f * FMath::Sin(Clock * 0.9f + Bird.Phase), 0.f));
}

void ATN_BeachGullZone::VisualTick(float DeltaSeconds)
{
	using namespace TNBeachGull;
	Clock += DeltaSeconds;
	const double Now = ServerNow(this);
	FVector View = GetActorLocation();
	TNBeachKit::LocalCamera(GetWorld(), View);
	const bool bNear = ViewDistance < GetVisualRange();
	const float Dt = FMath::Max(DeltaSeconds, 1e-3f);

	for (int32 i = 0; i < Birds.Num(); ++i)
	{
		FBird& Bird = Birds[i];
		const bool bAttacking = Attack.Kind != 0 && Attack.Bird == i;
		const float Tau = bAttacking ? static_cast<float>(Now - static_cast<double>(Attack.StartTime)) : 0.f;
		const FVector NewPos = bAttacking ? AttackPos(Bird, Now) : CirclePos(Bird, Now);
		const FVector Vel = (NewPos - Bird.Pos) / Dt;
		const bool bFirst = Bird.Pos.IsZero();
		Bird.Vel = bFirst ? FVector::ZeroVector : FMath::Lerp(Bird.Vel, Vel, static_cast<double>(1.f - FMath::Exp(-Dt / 0.15f)));
		Bird.Pos = NewPos;
		if (Bird.Vel.SizeSquared2D() > 2500.0)
		{
			const float NewYaw = static_cast<float>(Bird.Vel.Rotation().Yaw);
			const float YawRate = FMath::FindDeltaAngleDegrees(Bird.Yaw, NewYaw) / Dt;
			Bird.Yaw = NewYaw;
			Bird.Bank = FMath::FInterpTo(Bird.Bank, FMath::Clamp(YawRate * 0.3f, -40.f, 40.f), Dt, 3.f);
		}
		const float Climb = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Bird.Vel.Z, FMath::Max(1.0, Bird.Vel.Size2D()))));
		Bird.Pitch = FMath::FInterpTo(Bird.Pitch, FMath::Clamp(Climb, -60.f, 45.f), Dt, 4.f);
		if (BirdRoots.IsValidIndex(i) && BirdRoots[i])
		{
			BirdRoots[i]->SetWorldTransform(FTransform(FRotator(Bird.Pitch, Bird.Yaw, Bird.Bank), Bird.Pos, FVector(Bird.Scale)));
		}
		if (bNear)
		{
			PoseBird(i, DeltaSeconds, bAttacking, Tau);
		}

		// Sombra en la arena: más grande y cercana cuanto más baja el pájaro.
		Bird.ShadowTimer -= DeltaSeconds;
		if (Bird.ShadowTimer <= 0.f)
		{
			Bird.ShadowTimer = 0.25f;
			Bird.ShadowZ = GroundAt(Bird.Pos);
		}
		const float Height = static_cast<float>(Bird.Pos.Z) - Bird.ShadowZ;
		const float ShadowR = bNear ? Bird.Span * 0.32f * FMath::Clamp(1.5f - Height / 9000.f, 0.35f, 1.3f) : 0.f;
		if (Shadows.IsValidIndex(i))
		{
			TNBeachKit::PlaceShadow(Shadows[i], FVector(Bird.Pos.X, Bird.Pos.Y, Bird.ShadowZ), ShadowR);
		}

		// Graznidos sueltos y el silbido del picado.
		if (Voice && bNear)
		{
			Bird.SquawkTimer -= DeltaSeconds;
			if (Bird.SquawkTimer <= 0.f)
			{
				Bird.SquawkTimer = 5.f + 7.f * TNBeachKit::Hash01(static_cast<uint32>(Clock * 100.f) + static_cast<uint32>(i) * 97u);
				Voice->SetWorldLocation(Bird.Pos);
				Voice->Play(ETNBeachSfx::Squawk, (Bird.bPelican ? 0.55f : 1.f) * (0.9f + 0.2f * TNBeachKit::Hash01(static_cast<uint32>(i) + 5u)), 0.7f);
			}
			if (bAttacking && Attack.Kind == 2 && !bSwoopPlayed && Tau >= ClimbTime)
			{
				bSwoopPlayed = true;
				Voice->SetWorldLocation(Bird.Pos);
				Voice->Play(ETNBeachSfx::Swoop, Bird.bPelican ? 0.7f : 1.f, 1.3f);
			}
		}
	}

	// Cagada que cae y su sombra que se encoge.
	bool bShowDrop = false;
	if (Attack.Kind == 1 && Dropping)
	{
		const float Tau = static_cast<float>(Now - static_cast<double>(Attack.StartTime));
		if (Tau >= DropTime && Tau < DropTime + FallTime && Attack.Result == 0)
		{
			const ATortugaCharacter* Victim = Attack.Victim;
			FVector Target = Attack.bLocked ? FVector(Attack.Aim) : (Victim ? Victim->GetActorLocation() : FVector(Attack.Aim));
			if (!Attack.bLocked)
			{
				Target.Z = GroundAt(Target);
			}
			const float U = (Tau - DropTime) / FallTime;
			const FVector Top = Target + FVector(0.0, 0.0, PoopHeight);
			const FVector At = FMath::Lerp(Top, Target, static_cast<double>(FMath::Pow(U, 1.7f)));
			Dropping->SetWorldTransform(FTransform(FRotator(Clock * 90.f, Clock * 140.f, 0.f), At, FVector(SizeK)));
			TNBeachKit::PlaceShadow(DropShadow, Target, FMath::Lerp(600.f, SplatRadius * SizeK, FMath::Pow(U, 1.2f)));
			bShowDrop = true;
		}
	}
	if (Dropping && Dropping->IsVisible() != bShowDrop)
	{
		Dropping->SetVisibility(bShowDrop);
	}
	if (!bShowDrop)
	{
		TNBeachKit::PlaceShadow(DropShadow, FVector::ZeroVector, 0.f);
	}

	// Manchas: duran su vida y se encogen el último segundo.
	for (int32 s = Splats.Num() - 1; s >= 0; --s)
	{
		UStaticMeshComponent* Splat = Splats[s];
		const float Age = Clock - SplatBorn[s];
		if (!Splat || Age >= SplatLife[s])
		{
			if (Splat)
			{
				Splat->DestroyComponent();
			}
			Splats.RemoveAt(s);
			SplatBorn.RemoveAt(s);
			SplatLife.RemoveAt(s);
			SplatScale.RemoveAt(s);
			continue;
		}
		const float Left = SplatLife[s] - Age;
		if (Left < 1.f)
		{
			const float K = SplatScale[s] * FMath::Max(0.01f, Left);
			if (Splat->IsUsingAbsoluteScale())
			{
				Splat->SetWorldScale3D(FVector(K, K, SplatScale[s]));
			}
			else
			{
				Splat->SetRelativeScale3D(FVector(K));
			}
		}
	}

	TNBeachKit::TickEmitterIfBusy(Droplets, DeltaSeconds, View);
	TNBeachKit::TickEmitterIfBusy(Feathers, DeltaSeconds, View);
}
