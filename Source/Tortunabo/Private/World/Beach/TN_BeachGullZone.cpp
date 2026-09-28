#include "World/Beach/TN_BeachGullZone.h"
#include "World/Beach/TN_BeachCameraShake.h"
#include "World/Beach/TN_BeachEnemySynth.h"
#include "World/Beach/TN_BeachRaceGenerator.h"
#include "World/Beach/TN_BeachStun.h"
#include "TN_BeachEnemyKit.h"
#include "TN_BeachEnemyMeshes.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/TN_Log.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/TN_CarryComponent.h"
#include "Player/TN_TurtleAnimInstance.h"
#include "Player/TortugaCharacter.h"

namespace TNBeachGull
{
	/** Tiempo entre ataques (s) mientras haya tortugas debajo. */
	constexpr float AttackMin = 3.f;
	constexpr float AttackMax = 6.f;

	// ── Cagada ──
	/** Vuela sobre la tortuga (s), la cagada cae en FallTime desde PoopHeight (cm) y el ataque acaba en PoopEnd. */
	constexpr float DropTime = 1.5f;
	constexpr float FallTime = 1.35f;
	constexpr float PoopEnd = 4.6f;
	constexpr float PoopHeight = 3000.f;
	/** Radio de la mancha (cm, por el tamaño), tamaño del pegote que cae y adelanto al apuntar (s). */
	constexpr float SplatRadius = 280.f;
	constexpr float DropScale = 1.5f;
	constexpr float LeadPoop = 0.3f;
	/** Derribo de la manchada (s), su empujón (cm/s) y tiempo que la zona la deja en paz. */
	constexpr float PoopKnock = 2.4f;
	constexpr float PoopPush = 240.f;
	constexpr float PoopIgnore = 6.f;

	// ── Picado ──
	/** Sube y se coloca, deja de corregir, abre el pico (antes de llegar) y llega abajo (s). */
	constexpr float ClimbTime = 1.f;
	constexpr float LockTime = 1.9f;
	constexpr float StrikeTime = 2.45f;
	constexpr float JawLead = 0.45f;
	/**
	 * De dónde arranca el picado (cm desde la tortuga: casi encima, para que baje en picado de verdad y su sombra caiga
	 * sobre ella), radio en el que la coge (por el tamaño) y adelanto (s).
	 */
	constexpr float DiveStartDist = 1800.f;
	constexpr float DiveStartHeight = 4600.f;
	constexpr float GrabRadius = 300.f;
	constexpr float LeadDive = 0.25f;
	/** Al llegar abajo frena levantando el morro (grados) con la cabeza gacha. */
	constexpr float StrikePitch = 12.f;
	constexpr float StrikeHeadPitch = -30.f;
	/**
	 * Picado fallido: desde StrikeTime baja en PeckDown s hasta clavar el pico en la arena (con el morro y la cabeza hacia
	 * abajo), pica hasta PeckHold y remonta.
	 */
	constexpr float PeckDown = 0.14f;
	constexpr float PeckHold = 0.55f;
	constexpr float PeckPitch = -6.f;
	constexpr float PeckHeadPitch = -55.f;
	/** Mareada: cae a la arena dando tumbos (s) y, pasado el mareo, despega hacia su círculo (s). */
	constexpr float DazeFall = 0.8f;
	constexpr float DazeTakeOff = 1.8f;
	/** Solo se le puede dar con algo lanzado si su cuerpo está a menos de esto sobre la arena (cm). */
	constexpr float HittableHeight = 1800.f;

	// ── Agarre ──
	/**
	 * Con la tortuga en el pico: tirón (s), sube aleteando fuerte hasta RiseEnd, vuela y la suelta en CarryTime. La sube
	 * CarryHeight y la lleva CarryBack hacia la salida (cm). El pico pasa de la pose del picado a la del agarre en
	 * CatchBlend (s).
	 */
	constexpr float TugTime = 0.35f;
	constexpr float RiseEnd = 2.2f;
	constexpr float CarryTime = 3.3f;
	constexpr float CarryHeight = 2600.f;
	constexpr float CarryBack = 1500.f;
	constexpr float CatchBlend = 0.15f;
	/** Al soltarla: empujón de la bola hacia la salida (cm/s) y aturdimiento tras caer (s). */
	constexpr float ReleaseLaunch = 350.f;
	constexpr float AfterDropStun = 2.f;
	constexpr float GrabIgnore = 12.f;
	constexpr float DiveEndHit = StrikeTime + CarryTime + 2.4f;
	constexpr float DiveEndMiss = StrikeTime + PeckHold + 2.9f;
	constexpr float Gravity = 980.f;
	/** Del hueso de la espalda de la tortuga (Spine2) a la superficie del caparazón que muerde el pico (cm). */
	constexpr float ShellBack = 35.f;

	// ── Vuelo en círculos ──
	/** Capas de altura (cm sobre la zona): una por pájaro, barajadas, separadas LayerStep. */
	constexpr float LayerBase = 3200.f;
	constexpr float LayerStep = 800.f;
	/** Velocidad por el círculo (cm/s) y lo que deriva el centro de cada círculo (cm). */
	constexpr float GullSpeedMin = 900.f;
	constexpr float GullSpeedMax = 1300.f;
	constexpr float PelicanSpeedMin = 700.f;
	constexpr float PelicanSpeedMax = 900.f;
	constexpr float Drift = 700.f;

	inline float Smooth01(float X)
	{
		const float C = FMath::Clamp(X, 0.f, 1.f);
		return C * C * (3.f - 2.f * C);
	}

	/** Hueso de la espalda de la tortuga por el que la sujeta el pico. */
	inline FName SpineBone()
	{
		static const FName Name(TEXT("Spine2"));
		return Name;
	}

	/**
	 * Coloca una sombra de pájaro: en Ground con radio Radius (0 la esconde), opacidad Opacity y nitidez Sharp (0 borde
	 * difuminado, 1 nítido; cambia de malla por tramos). Bucket guarda la malla que lleva.
	 */
	inline void PlaceBirdShadow(UStaticMeshComponent* Comp, int32& Bucket, const FVector& Ground, float Radius, float Opacity, float Sharp)
	{
		if (!Comp)
		{
			return;
		}
		TNBeachKit::PlaceShadow(Comp, Ground, Radius);
		if (Radius <= 1.f)
		{
			return;
		}
		static const float InnerFrac[3] = { 0.35f, 0.6f, 0.85f };
		const int32 Want = Sharp < 0.33f ? 0 : (Sharp < 0.66f ? 1 : 2);
		if (Want != Bucket)
		{
			Bucket = Want;
			Comp->SetStaticMesh(TNBeachKit::ShadowDiscEdge(InnerFrac[Want]));
		}
		TNBeachKit::SetOpacity(TNBeachKit::SoftMID(Comp), Opacity);
	}
}

ATN_BeachGullZone::ATN_BeachGullZone()
{
	bUsesMover = false;
	NetFrequencyNear = 10.f;
	SetNetUpdateFrequency(NetFrequencyNear);
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
	if (const ATN_BeachRaceGenerator* Gen = FindGenerator())
	{
		const FVector Sea = Gen->GetSeaDirection().GetSafeNormal2D();
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
	EndHoldLocal();
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

void ATN_BeachGullZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Después de mover los pájaros: la tortuga que va en el pico, en el pico.
	TickHold();
}

// ─────────────────────────────────────────────────────────────────────────────
// Pájaros
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachGullZone::BuildBirds()
{
	using namespace TNBeachGull;
	if (Birds.Num() > 0)
	{
		return;
	}
	// Parámetros de vuelo en todas las máquinas (los mismos con la misma semilla).
	const uint32 Seed = static_cast<uint32>(Spec.Seed);
	const int32 NumGulls = 3 + (TNBeachKit::Hash01(Seed * 5u + 2u) < 0.5f ? 1 : 0);
	const bool bWithPelican = TNBeachKit::Hash01(Seed * 3u + 1u) < 0.6f;
	const int32 Count = NumGulls + (bWithPelican ? 1 : 0);
	const float Turn = TNBeachKit::Hash01(Seed * 7u + 5u) < 0.5f ? 1.f : -1.f;
	// Una capa de altura por pájaro, barajadas: nunca dos a la misma altura.
	TArray<int32> HeightLayers;
	for (int32 k = 0; k < Count; ++k)
	{
		HeightLayers.Add(k);
	}
	for (int32 k = Count - 1; k > 0; --k)
	{
		const int32 j = FMath::Min(k, static_cast<int32>(TNBeachKit::Hash01(Seed * 11u + static_cast<uint32>(k) * 13u) * static_cast<float>(k + 1)));
		HeightLayers.Swap(k, j);
	}
	// Centros repartidos alrededor del de la zona con el ángulo áureo: cada uno en su sitio.
	const float StartAngle = 2.f * PI * TNBeachKit::Hash01(Seed * 17u + 3u);
	for (int32 b = 0; b < Count; ++b)
	{
		const uint32 B = static_cast<uint32>(b);
		FBird& Bird = Birds.AddDefaulted_GetRef();
		Bird.bPelican = bWithPelican && b == Count - 1;
		const float CenterAngle = StartAngle + 2.39996f * static_cast<float>(b);
		const float CenterDist = CircleRadius * FMath::Lerp(0.12f, 0.6f, TNBeachKit::Hash01(Seed + B * 19u + 7u));
		Bird.CenterOffset = FVector2D(FMath::Cos(CenterAngle), FMath::Sin(CenterAngle)) * CenterDist;
		Bird.Radius = FMath::Max(1800.f, CircleRadius * FMath::Lerp(0.45f, 0.9f, TNBeachKit::Hash01(Seed + B * 17u)));
		Bird.Ratio = FMath::Lerp(0.65f, 1.f, TNBeachKit::Hash01(Seed + B * 23u + 1u));
		Bird.OvalYaw = 2.f * PI * TNBeachKit::Hash01(Seed + B * 29u + 5u);
		Bird.Height = LayerBase + LayerStep * static_cast<float>(HeightLayers[b]) + 400.f * (TNBeachKit::Hash01(Seed + B * 31u) - 0.5f);
		Bird.Phase = 2.f * PI * TNBeachKit::Hash01(Seed + B * 13u);
		// Casi todas giran hacia el mismo lado; alguna, al revés.
		const float Dir = TNBeachKit::Hash01(Seed + B * 37u + 9u) < 0.3f ? -Turn : Turn;
		const float SpeedK = TNBeachKit::Hash01(Seed + B * 47u + 4u);
		const float Speed = Bird.bPelican ? FMath::Lerp(PelicanSpeedMin, PelicanSpeedMax, SpeedK) : FMath::Lerp(GullSpeedMin, GullSpeedMax, SpeedK);
		Bird.AngSpeed = Dir * Speed / Bird.Radius;
		Bird.DriftPhase = 2.f * PI * TNBeachKit::Hash01(Seed + B * 41u + 2u);
		Bird.Scale = (Bird.bPelican ? 24.f : 28.f) * FMath::Sqrt(SizeK);
		Bird.Span = (Bird.bPelican ? 170.f : 90.f) * Bird.Scale;
		Bird.SquawkTimer = 2.f + 6.f * TNBeachKit::Hash01(Seed + B * 43u);
	}
	if (!bHasScreen)
	{
		return;
	}

	// Mallas de la fauna (gaviota y pelícano), compartidas por pieza, y la mandíbula de abajo de cada una.
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
		// Mandíbula de abajo en la base del pico: se abre para coger a la tortuga y para graznar.
		UStaticMeshComponent* HeadComp = BirdParts.IsValidIndex(Bird.Head) ? BirdParts[Bird.Head].Get() : nullptr;
		const bool bPelican = Bird.bPelican;
		UStaticMesh* JawMesh = TNBeachKit::CachedMesh(bPelican ? TEXT("Beach.Pelican.Jaw") : TEXT("Beach.Gull.Jaw"),
			[bPelican](TNProcMesh::FTNProcMeshBuffers& M) { TNBeachMeshes::BuildBirdJaw(M, bPelican); });
		Jaws.Add(HeadComp ? TNBeachKit::AddPart(this, HeadComp, JawMesh, TNBeachMeshes::BirdGeom(bPelican).BeakBase, false) : nullptr);

		UStaticMeshComponent* Shadow = TNBeachKit::AddShadow(this, 0.38f);
		TNBeachKit::PlaceShadow(Shadow, FVector::ZeroVector, 0.f);
		Shadows.Add(Shadow);
		Bird.ShadowZ = static_cast<float>(GetActorLocation().Z);
		Bird.ShadowTimer = 0.05f * static_cast<float>(b);
	}

	UStaticMesh* DropMesh = TNBeachKit::CachedMesh(TEXT("Beach.Dropping"), [](TNProcMesh::FTNProcMeshBuffers& M) { TNBeachMeshes::BuildDropping(M); });
	Dropping = TNBeachKit::AddPart(this, GetRootComponent(), DropMesh, FVector::ZeroVector, true);
	if (Dropping)
	{
		Dropping->SetAbsolute(true, true, true);
		Dropping->SetVisibility(false);
	}
	DropShadow = TNBeachKit::AddShadow(this, 0.6f);
	TNBeachKit::PlaceShadow(DropShadow, FVector::ZeroVector, 0.f);

	using TNAmbientFX::EShape;
	TNAmbientFX::FEmitterDesc DropDesc = TNBeachKit::MakeDesc(EShape::Drop, FLinearColor(0.97f, 0.97f, 0.94f), false, 1.f, 40, 0.f, 1000.f, -1800.f, 0.5f, 1.f, 55.f, 40.f);
	DropDesc.Spread = 0.9f;
	DropDesc.SpawnRadius = 150.f;
	TNBeachKit::InitEmitter(Droplets, this, DropDesc, Seed + 51u);
	TNAmbientFX::FEmitterDesc FeatherDesc = TNBeachKit::MakeDesc(EShape::Flake, FLinearColor(0.95f, 0.95f, 0.93f), false, 1.f, 30, 0.f, 500.f, -120.f, 1.5f, 3.f, 90.f, 70.f);
	FeatherDesc.Spread = 1.f;
	FeatherDesc.SpawnRadius = 300.f;
	TNBeachKit::InitEmitter(Feathers, this, FeatherDesc, Seed + 52u);
	// Estela de gotitas que deja la cagada al caer.
	TNAmbientFX::FEmitterDesc TrailDesc = TNBeachKit::MakeDesc(EShape::Drop, FLinearColor(0.98f, 0.98f, 0.95f), false, 1.f, 40, 0.f, 80.f, -300.f, 0.35f, 0.6f, 45.f, 12.f);
	TrailDesc.Spread = 0.5f;
	TrailDesc.SpawnRadius = 40.f;
	TNBeachKit::InitEmitter(Trail, this, TrailDesc, Seed + 53u);
	// Arena que levantan las alas al coger a una tortuga.
	TNAmbientFX::FEmitterDesc SandDesc = TNBeachKit::MakeDesc(EShape::Puff, FLinearColor(0.86f, 0.76f, 0.56f), true, 0.55f, 30, 0.f, 700.f, -60.f, 1.f, 2.f, 250.f, 600.f);
	SandDesc.Spread = 1.f;
	SandDesc.SpawnRadius = 400.f;
	TNBeachKit::InitEmitter(SandPuff, this, SandDesc, Seed + 54u);
	// Granos de arena que saltan al picar en el sitio (el picado fallido).
	TNAmbientFX::FEmitterDesc PeckDesc = TNBeachKit::MakeDesc(EShape::Ember, FLinearColor(0.82f, 0.72f, 0.52f), false, 1.f, 40, 0.f, 1100.f, -1500.f, 0.6f, 1.1f, 30.f, 22.f);
	PeckDesc.Spread = 0.8f;
	PeckDesc.SpawnRadius = 90.f;
	TNBeachKit::InitEmitter(PeckSand, this, PeckDesc, Seed + 55u);
	GetVoice(GetRootComponent(), 2500.f, 16000.f);
}

FVector ATN_BeachGullZone::CirclePos(const FBird& Bird, double Now) const
{
	// Óvalo propio alrededor de su centro (desplazado del de la zona y que deriva despacio) y a su altura.
	const double A = Bird.Phase + Bird.AngSpeed * Now;
	const double Lx = FMath::Cos(A) * Bird.Radius;
	const double Ly = FMath::Sin(A) * Bird.Radius * Bird.Ratio;
	const double Co = FMath::Cos(Bird.OvalYaw);
	const double So = FMath::Sin(Bird.OvalYaw);
	const double Dx = TNBeachGull::Drift * FMath::Sin(0.05 * Now + Bird.DriftPhase);
	const double Dy = TNBeachGull::Drift * FMath::Cos(0.041 * Now + Bird.DriftPhase * 1.3);
	return GetActorLocation() + FVector(Bird.CenterOffset.X + Lx * Co - Ly * So + Dx, Bird.CenterOffset.Y + Lx * So + Ly * Co + Dy,
		Bird.Height + 250.0 * FMath::Sin(0.37 * Now + Bird.Phase * 3.0));
}

FVector ATN_BeachGullZone::AttackPos(const FBird& Bird, double Now, float Tau) const
{
	using namespace TNBeachGull;
	const FVector Circle = CirclePos(Bird, Now);
	if (Attack.Kind == 3)
	{
		// Mareada: cae a la arena dando tumbos, se queda sentada lo que dura el mareo y despega hacia su círculo.
		const FVector HitAt = Attack.Aim;
		const FVector Sit = Attack.Hold;
		const float StunFor = FMath::Max(DazeFall, HitStunEndTime - Attack.StartTime);
		if (Tau < DazeFall)
		{
			const float A = Tau / DazeFall;
			return FMath::Lerp(HitAt, Sit, static_cast<double>(A * A)) + FVector(0.0, 0.0, 300.0 * FMath::Sin(PI * A) * (1.f - A));
		}
		if (Tau < StunFor)
		{
			return Sit;
		}
		const float B = Smooth01((Tau - StunFor) / DazeTakeOff);
		return FMath::Lerp(Sit, Circle, static_cast<double>(B)) + FVector(0.0, 0.0, 900.0 * FMath::Sin(PI * B));
	}
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

	// Picado: se coloca lejos y alto, se lanza acelerando hacia la tortuga y frena con el pico en su caparazón.
	FVector Away = From - Target;
	Away.Z = 0.0;
	Away = Away.IsNearlyZero() ? FVector::ForwardVector : Away.GetSafeNormal();
	const FVector DiveStart = Target + Away * DiveStartDist + FVector(0.0, 0.0, DiveStartHeight);
	if (Tau < ClimbTime)
	{
		return FMath::Lerp(From, DiveStart, static_cast<double>(Smooth01(Tau / ClimbTime)));
	}
	const FVector Strike = StrikeRoot(Bird, Now);
	if (Tau < StrikeTime || Attack.Result == 0)
	{
		const float U = FMath::Clamp((Tau - ClimbTime) / (StrikeTime - ClimbTime), 0.f, 1.f);
		return FMath::Lerp(DiveStart, Strike, static_cast<double>(U * U));
	}
	if (Attack.Result == 1)
	{
		// Tras soltarla (el agarre lo coloca VisualTick): remonta desde donde la soltó y vuelve a su vuelta.
		const FVector Release = RootForGrip(Bird, GripPath(CarryTime), CarryRotation(CarryTime), CarryHeadPitch(CarryTime));
		const float B = Smooth01((Tau - StrikeTime - CarryTime) / (DiveEndHit - StrikeTime - CarryTime));
		return FMath::Lerp(Release + FVector(0.0, 0.0, 1500.0 * B), Circle, static_cast<double>(B));
	}
	// Fallo: baja igual hasta clavar el pico en la arena (o en lo que la cubría), pica dos veces, remonta de largo y vuelve.
	const float U = Tau - StrikeTime;
	const FVector Peck = PeckRoot(Bird);
	if (U < PeckDown)
	{
		return FMath::Lerp(Strike, Peck, static_cast<double>(Smooth01(U / PeckDown)));
	}
	if (U < PeckHold)
	{
		// Picotazos: el cuerpo sube y baja un poco con cada uno.
		return Peck + FVector(0.0, 0.0, 45.0 * FMath::Abs(FMath::Sin((U - PeckDown) * 14.f)));
	}
	FVector Fwd = Target - DiveStart;
	Fwd.Z = 0.0;
	Fwd = Fwd.IsNearlyZero() ? FVector::ForwardVector : Fwd.GetSafeNormal();
	const float K = FMath::Min(U - PeckHold, 0.9f);
	const FVector Pull = Peck + Fwd * (K * 3000.0) + FVector(0.0, 0.0, K * K * 3500.0);
	if (U < PeckHold + 0.9f)
	{
		return Pull;
	}
	const float B = Smooth01((U - PeckHold - 0.9f) / (DiveEndMiss - StrikeTime - PeckHold - 0.9f));
	return FMath::Lerp(Pull, Circle, static_cast<double>(B));
}

float ATN_BeachGullZone::GroundAt(const FVector& Where) const
{
	// La arena del generador, sin trazas (un pájaro que se abre por encima de la selva cruzaría los muros invisibles de
	// los lados, y una traza que empieza dentro de uno da en su punto de partida); sin generador, traza.
	float Z = static_cast<float>(GetActorLocation().Z);
	GroundHeightAt(FVector(Where.X, Where.Y, FMath::Max(Where.Z, GetActorLocation().Z)), Z);
	return Z;
}

// ─────────────────────────────────────────────────────────────────────────────
// Agarre: las mismas cuentas en todas las máquinas
// ─────────────────────────────────────────────────────────────────────────────

FVector ATN_BeachGullZone::GripOffset(const FBird& Bird, float HeadPitch) const
{
	const TNBeachMeshes::FBirdGeom G = TNBeachMeshes::BirdGeom(Bird.bPelican);
	return G.BodyPivot + G.HeadPivot + FRotator(HeadPitch, 0.f, 0.f).RotateVector(G.Grip);
}

FVector ATN_BeachGullZone::RootForGrip(const FBird& Bird, const FVector& Grip, const FRotator& Rot, float HeadPitch) const
{
	return Grip - Rot.RotateVector(GripOffset(Bird, HeadPitch) * Bird.Scale);
}

float ATN_BeachGullZone::GripDropFor(const ATortugaCharacter* Turtle) const
{
	const UCapsuleComponent* Capsule = Turtle ? Turtle->GetCapsuleComponent() : nullptr;
	return Capsule ? Capsule->GetScaledCapsuleHalfHeight() * 0.75f : 70.f;
}

float ATN_BeachGullZone::HeldYaw() const
{
	return static_cast<float>(CourseBack.Rotation().Yaw);
}

FVector ATN_BeachGullZone::GripPath(float U) const
{
	using namespace TNBeachGull;
	const FVector Start = FVector(Attack.Hold) + FVector(0.0, 0.0, GripDropFor(Attack.Victim));
	// Tirón hacia arriba (la tortuga se resiste), subida aleteando y un vuelo corto meciéndola antes de soltarla.
	const float Rise = Smooth01((U - TugTime) / (RiseEnd - TugTime));
	const float Tug = U < TugTime ? 110.f * FMath::Sin(PI * U / TugTime) : 0.f;
	const float Glide = U > RiseEnd ? 80.f * FMath::Sin((U - RiseEnd) * 6.f) : 0.f;
	const float Along = FMath::Pow(FMath::Clamp((U - 0.2f) / (CarryTime - 0.2f), 0.f, 1.f), 1.5f);
	return Start + CourseBack * (CarryBack * Along) + FVector(0.0, 0.0, CarryHeight * Rise + Tug + Glide);
}

FRotator ATN_BeachGullZone::CarryRotation(float U) const
{
	using namespace TNBeachGull;
	const FVector A = GripPath(FMath::Max(0.f, U - 0.05f));
	const FVector B = GripPath(U + 0.05f);
	const FVector D = B - A;
	float Pitch = FMath::Clamp(0.6f * FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(D.Z), FMath::Max(1.f, static_cast<float>(D.Size2D())))), -10.f, 30.f);
	if (U < TugTime)
	{
		// Tirando de ella hacia arriba con el morro levantado.
		Pitch = FMath::Lerp(StrikePitch, 25.f, U / TugTime);
	}
	return FRotator(Pitch, HeldYaw(), 6.f * FMath::Sin(U * 7.f));
}

float ATN_BeachGullZone::CarryHeadPitch(float U) const
{
	using namespace TNBeachGull;
	// Cabeza gacha con la tortuga en el pico, sacudiéndola un poco (más en el tirón).
	return U < TugTime ? -30.f + 10.f * FMath::Sin(U * 14.f) : -38.f + 6.f * FMath::Sin(U * 9.f);
}

FVector ATN_BeachGullZone::StrikeRoot(const FBird& Bird, double Now) const
{
	using namespace TNBeachGull;
	const ATortugaCharacter* Victim = Attack.Victim;
	const float Drop = GripDropFor(Victim);
	FVector Grip;
	if (Attack.Kind == 2 && Attack.Result == 2)
	{
		// Falla: el pico llega justo encima de donde va a picar (la arena o la sombrilla que la cubría).
		Grip = FVector(Attack.Hold) + FVector(0.0, 0.0, 160.0);
	}
	else if (Attack.bLocked)
	{
		// Aim está en el suelo: el caparazón, media cápsula más arriba.
		const UCapsuleComponent* Capsule = Victim ? Victim->GetCapsuleComponent() : nullptr;
		const float Half = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.f;
		Grip = FVector(Attack.Aim) + FVector(0.0, 0.0, Half + Drop);
	}
	else
	{
		Grip = (Victim ? Victim->GetActorLocation() : FVector(Attack.Aim)) + FVector(0.0, 0.0, Drop);
	}
	return RootForGrip(Bird, Grip, FRotator(StrikePitch, DiveYaw(Bird, Grip), 0.f), StrikeHeadPitch);
}

float ATN_BeachGullZone::DiveYaw(const FBird& Bird, const FVector& Target) const
{
	FVector Dir = Target - CirclePos(Bird, Attack.StartTime);
	Dir.Z = 0.0;
	return Dir.IsNearlyZero() ? 0.f : static_cast<float>(Dir.Rotation().Yaw);
}

FVector ATN_BeachGullZone::PeckRoot(const FBird& Bird) const
{
	using namespace TNBeachGull;
	// El pico clavado en la arena (un pelo por encima del punto), con el morro y la cabeza hacia abajo.
	const FVector Grip = FVector(Attack.Hold) + FVector(0.0, 0.0, 15.0);
	return RootForGrip(Bird, Grip, FRotator(PeckPitch, DiveYaw(Bird, Grip), 0.f), PeckHeadPitch);
}

void ATN_BeachGullZone::BirdPose(int32 Index, double Now, FVector& OutRoot, FRotator& OutRot) const
{
	using namespace TNBeachGull;
	const FBird& Bird = Birds[Index];
	const bool bAttacking = Attack.Kind != 0 && Attack.Bird == Index;
	const float Tau = bAttacking ? static_cast<float>(Now - static_cast<double>(Attack.StartTime)) : 0.f;
	const float U = Tau - StrikeTime;
	if (bAttacking && Attack.Kind == 2 && Attack.Result == 1 && U >= 0.f && U < CarryTime)
	{
		OutRot = CarryRotation(U);
		OutRoot = RootForGrip(Bird, GripPath(U), OutRot, CarryHeadPitch(U));
		return;
	}
	OutRoot = bAttacking ? AttackPos(Bird, Now, Tau) : CirclePos(Bird, Now);
	// El giro que se ve (en el servidor sin pantalla, solo el rumbo del picado): basta para su cuerpo.
	OutRot = FRotator(Bird.Pitch, Bird.Yaw, Bird.Bank);
	if (!bHasScreen && bAttacking && Attack.Kind == 2)
	{
		OutRot = FRotator(-40.f, DiveYaw(Bird, FVector(Attack.Aim)), 0.f);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Servidor
// ─────────────────────────────────────────────────────────────────────────────

int32 ATN_BeachGullZone::PickBird(const FVector& Where, bool bForPoop) const
{
	const double Now = ServerNow(this);
	int32 Best = INDEX_NONE;
	double BestSq = 1.0e18;
	for (int32 b = 0; b < Birds.Num(); ++b)
	{
		if (bForPoop && Birds[b].bPelican)
		{
			continue;
		}
		const double DistSq = FVector::DistSquared2D(CirclePos(Birds[b], Now), Where);
		if (DistSq < BestSq)
		{
			BestSq = DistSq;
			Best = b;
		}
	}
	return Best;
}

void ATN_BeachGullZone::DebugAttackNow(int32 InKind)
{
	if (!HasAuthority() || Birds.Num() == 0 || Attack.Kind != 0)
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
		if (CanBeHit(Turtle) && DistSq < BestSq)
		{
			BestSq = DistSq;
			Best = Turtle;
		}
	}
	if (!Best)
	{
		return;
	}
	const uint8 Kind = InKind == 1 || InKind == 2 ? static_cast<uint8>(InKind) : static_cast<uint8>(ServerRng.FRand() < 0.5f ? 1 : 2);
	int32 BirdIndex = PickBird(Best->GetActorLocation(), Kind == 1);
	if (BirdIndex == INDEX_NONE)
	{
		BirdIndex = 0;
	}
	StartAttack(Best, Kind, BirdIndex);
}

void ATN_BeachGullZone::StartAttack(ATortugaCharacter* Victim, uint8 InKind, int32 BirdIndex)
{
	Attack.Victim = Victim;
	Attack.Aim = Victim ? Victim->GetActorLocation() : GetActorLocation();
	Attack.Hold = FVector::ZeroVector;
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
	if (HeldLocal.IsValid())
	{
		EndHoldLocal();
	}
	Attack.Kind = 0;
	Attack.Victim = nullptr;
	NextAttackTime = Now + ServerRng.FRandRange(TNBeachGull::AttackMin, TNBeachGull::AttackMax);
	ForceNetUpdate();
	OnAttackChanged();
}

void ATN_BeachGullZone::ServerTick(float DeltaSeconds)
{
	const double Now = ServerNow(this);
	if (Attack.Kind == 3)
	{
		// Mareada en la arena: cuando se le pasa y ha despegado, vuelve a su círculo (y la zona, a atacar).
		if (Now > static_cast<double>(HitStunEndTime) + TNBeachGull::DazeTakeOff)
		{
			EndAttack(Now);
		}
		return;
	}
	if (Attack.Kind != 0)
	{
		const float Tau = static_cast<float>(Now - static_cast<double>(Attack.StartTime));
		if (Attack.Kind == 1)
		{
			ServerPoop(Tau);
		}
		else
		{
			ServerDive(Tau);
		}
		if (Attack.Kind != 0 && Tau > 14.f)
		{
			EndAttack(Now);
		}
		return;
	}
	if (Now < NextAttackTime || !IsRaceLive(this) || Birds.Num() == 0 || IsHitStunned())
	{
		return;
	}
	// A por una de las tortugas que están debajo (sin sombrilla, sin aturdir, sin derribar).
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
	// Va el pájaro más cercano: la mitad de las veces una gaviota caga; si no, picado (el pelícano solo pica).
	const bool bPoop = ServerRng.FRand() < 0.5f;
	int32 BirdIndex = PickBird(Victim->GetActorLocation(), bPoop);
	uint8 Kind = static_cast<uint8>(bPoop ? 1 : 2);
	if (BirdIndex == INDEX_NONE)
	{
		BirdIndex = PickBird(Victim->GetActorLocation(), false);
		Kind = 2;
	}
	StartAttack(Victim, Kind, FMath::Max(0, BirdIndex));

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
		// Se fija dónde cae (con adelanto). La traza desde arriba da en el techo si lo hay: la cagada cae encima.
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
				if (!CanBeHit(Turtle) || Turtle->HasUmbrellaProtection())
				{
					continue;
				}
				const FVector At = Turtle->GetActorLocation();
				// A cubierto (bajo una sombrilla, en el castillo) la mancha cae encima, no en ella.
				if (FVector::Dist2D(At, Impact) > SplatRadius * SizeK + 45.f || FMath::Abs(At.Z - Impact.Z) > 300.0)
				{
					continue;
				}
				// El pegote la tumba de espaldas: derribo con ragdoll y mareo, un empujoncito hacia fuera.
				FVector Away = At - Impact;
				Away.Z = 0.0;
				Away = Away.IsNearlyZero() ? Turtle->GetActorForwardVector() * -1.0 : Away.GetSafeNormal();
				const FVector Spin = FVector::CrossProduct(FVector::UpVector, Away) * 200.0;
				KnockDownTurtle(Turtle, PoopKnock, Away * PoopPush + FVector(0.0, 0.0, 120.0), Spin);
				IgnoreTurtle(Turtle, PoopIgnore);
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

void ATN_BeachGullZone::ServerDive(float Tau)
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
		// Con algo encima (sombrilla, techo) no puede bajar: fallará y picará en lo que la cubre.
		FHitResult RoofHit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BeachGullRoof), false);
		if (GetWorld()->LineTraceSingleByObjectType(RoofHit, Point + FVector(0.0, 0.0, 3000.0), Point + FVector(0.0, 0.0, 250.0),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			Attack.Result = 2;
			Attack.Hold = RoofHit.ImpactPoint;
			OnAttackChanged();
		}
		ForceNetUpdate();
	}
	if (Attack.Result == 0 && Tau >= StrikeTime)
	{
		// La coge si está bajo el pico, de pie (ni en pleno panzazo, que la esquiva, ni en bola ni en brazos de otra).
		ATortugaCharacter* Caught = nullptr;
		if (IsRaceLive(this))
		{
			const FVector Point = Attack.Aim;
			float Best = GrabRadius * SizeK + 45.f;
			TArray<ATortugaCharacter*> Turtles;
			GatherTurtles(this, Turtles);
			for (ATortugaCharacter* Turtle : Turtles)
			{
				if (!IsTargetable(Turtle) || Turtle->HasUmbrellaProtection() || Turtle->IsBellyPoseActive() || Turtle->IsInShell())
				{
					continue;
				}
				const UTN_CarryComponent* Carry = Turtle->GetCarryComponent();
				if (Carry && Carry->IsBeingCarried())
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
			Attack.Hold = Caught->GetActorLocation();
			bReleased = false;
			IgnoreTurtle(Caught, GrabIgnore);
			// Si llevaba a otra en brazos, la suelta.
			if (UTN_CarryComponent* Carry = Caught->GetCarryComponent())
			{
				if (Carry->IsCarrying())
				{
					Carry->ForceRelease(false);
				}
			}
			// El movimiento se apaga ya en el servidor (los clientes, al recibir el ataque).
			BeginHoldLocal(Caught);
			UE_LOG(LogTortunabo, Log, TEXT("[Playa] %s coge a %s con el pico."), *GetName(), *GetNameSafe(Caught));
		}
		else
		{
			// Nadie bajo el pico: pica igual en la arena donde iba.
			Attack.Result = 2;
			Attack.Hold = Attack.Aim;
		}
		ForceNetUpdate();
		OnAttackChanged();
	}
	if (Attack.Result == 1 && !bReleased)
	{
		ATortugaCharacter* Carried = Attack.Victim;
		const float U = Tau - StrikeTime;
		const bool bGone = !IsValid(Carried) || Carried->IsDead();
		// Se escurre si se mete en el caparazón (o si algo la derriba); si no, la suelta al acabar el vuelo.
		const bool bSlipped = !bGone && (Carried->IsInShell() || Carried->IsKnockedDown());
		if (bGone || bSlipped || U >= CarryTime)
		{
			ReleaseCarried();
		}
	}
	if (Attack.Result != 0 && Tau >= (Attack.Result == 1 ? DiveEndHit : DiveEndMiss))
	{
		EndAttack(ServerNow(this));
	}
}

void ATN_BeachGullZone::ReleaseCarried()
{
	using namespace TNBeachGull;
	ATortugaCharacter* Carried = Attack.Victim;
	bReleased = true;
	EndHoldLocal();
	if (IsValid(Carried) && !Carried->IsDead() && !Carried->IsKnockedDown())
	{
		// Cae en bola y sigue aturdida un rato al llegar al suelo (si cae dentro de la tormenta, la patada la saca).
		float GroundZ = static_cast<float>(Attack.Hold.Z);
		GroundHeightAt(Carried->GetActorLocation(), GroundZ);
		const float Height = FMath::Max(0.f, static_cast<float>(Carried->GetActorLocation().Z) - GroundZ);
		const float FallSeconds = FMath::Sqrt(2.f * Height / Gravity);
		StunTurtle(Carried, FallSeconds + AfterDropStun, CourseBack * ReleaseLaunch + FVector(0.0, 0.0, -50.0));
	}
	ForceNetUpdate();
}

// ─────────────────────────────────────────────────────────────────────────────
// Mareo por lo que se le lanza
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachGullZone::ApplyHitStun(float Seconds, AActor* InstigatorActor)
{
	Super::ApplyHitStun(Seconds, InstigatorActor);
	if (!HasAuthority() || !IsHitStunned() || Birds.Num() == 0 || Attack.Kind == 3)
	{
		// Ya mareada: solo se alarga (el final va en HitStunEndTime).
		return;
	}
	// El pájaro que atacaba (o, sin ataque, el más cercano a lo que le ha dado).
	int32 BirdIndex = Attack.Kind != 0 ? static_cast<int32>(Attack.Bird) : INDEX_NONE;
	if (!Birds.IsValidIndex(BirdIndex))
	{
		BirdIndex = FMath::Max(0, PickBird(InstigatorActor ? InstigatorActor->GetActorLocation() : GetActorLocation(), false));
	}
	const double Now = ServerNow(this);
	FVector Root;
	FRotator Rot;
	BirdPose(BirdIndex, Now, Root, Rot);
	// Suelta a la que llevara en el pico.
	if (Attack.Kind == 2 && Attack.Result == 1 && !bReleased)
	{
		ReleaseCarried();
	}
	Attack.Kind = 3;
	Attack.Bird = static_cast<uint8>(BirdIndex);
	Attack.Victim = nullptr;
	Attack.Aim = Root;
	Attack.Hold = FVector(Root.X, Root.Y, GroundAt(Root));
	Attack.StartTime = static_cast<float>(Now);
	Attack.Result = 0;
	Attack.bLocked = 1;
	++Attack.Serial;
	ForceNetUpdate();
	OnAttackChanged();
}

bool ATN_BeachGullZone::GetHitCapsule(FVector& OutA, FVector& OutB, float& OutRadius) const
{
	using namespace TNBeachGull;
	if (!Birds.IsValidIndex(Attack.Bird))
	{
		return false;
	}
	const double Now = ServerNow(this);
	const float Tau = static_cast<float>(Now - static_cast<double>(Attack.StartTime));
	// Solo el que baja en picado (también picando en el sitio), el que lleva una tortuga y el ya mareado.
	bool bHittable = false;
	if (Attack.Kind == 2)
	{
		bHittable = Tau >= ClimbTime && (Attack.Result != 1 || Tau - StrikeTime < CarryTime);
	}
	else if (Attack.Kind == 3)
	{
		bHittable = true;
	}
	if (!bHittable)
	{
		return false;
	}
	const FBird& Bird = Birds[Attack.Bird];
	FVector Root;
	FRotator Rot;
	BirdPose(Attack.Bird, Now, Root, Rot);
	const TNBeachMeshes::FBirdGeom G = TNBeachMeshes::BirdGeom(Bird.bPelican);
	const FVector BodyAt = Root + Rot.RotateVector(G.BodyPivot * Bird.Scale);
	// A tiro de piedra: cerca de la arena.
	if (BodyAt.Z - GroundAt(BodyAt) > HittableHeight)
	{
		return false;
	}
	OutA = BodyAt;
	OutB = Root + Rot.RotateVector((G.BodyPivot + G.HeadPivot) * Bird.Scale);
	OutRadius = Bird.Span * 0.12f;
	return true;
}

FVector ATN_BeachGullZone::GetHitStunAnchor() const
{
	if (Attack.Kind == 3 && Birds.IsValidIndex(Attack.Bird))
	{
		const FBird& Bird = Birds[Attack.Bird];
		const TNBeachMeshes::FBirdGeom G = TNBeachMeshes::BirdGeom(Bird.bPelican);
		FVector Root = Bird.Pos;
		FRotator Rot(Bird.Pitch, Bird.Yaw, Bird.Bank);
		if (Root.IsZero())
		{
			BirdPose(Attack.Bird, ServerNow(this), Root, Rot);
		}
		return Root + Rot.RotateVector((G.BodyPivot + G.HeadPivot) * Bird.Scale) + FVector(0.0, 0.0, 12.0 * Bird.Scale);
	}
	return GetActorLocation() + FVector(0.0, 0.0, 3000.0);
}

float ATN_BeachGullZone::GetHitStunScale() const
{
	const bool bPelican = Birds.IsValidIndex(Attack.Bird) && Birds[Attack.Bird].bPelican;
	return (bPelican ? 9.f : 7.f) * SizeK;
}

// ─────────────────────────────────────────────────────────────────────────────
// La tortuga en el pico (todas las máquinas)
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachGullZone::BeginHoldLocal(ATortugaCharacter* Turtle)
{
	if (!Turtle || HeldLocal.Get() == Turtle)
	{
		return;
	}
	if (HeldLocal.IsValid())
	{
		EndHoldLocal();
	}
	HeldLocal = Turtle;
	if (UCharacterMovementComponent* Move = Turtle->GetCharacterMovement())
	{
		// Sin movimiento propio mientras cuelga: la coloca el pico en cada máquina con las mismas cuentas.
		Move->StopMovementImmediately();
		Move->DisableMovement();
		if (Turtle->GetLocalRole() == ROLE_SimulatedProxy)
		{
			SavedSmoothing = static_cast<uint8>(Move->NetworkSmoothingMode);
			bSmoothingSaved = true;
			Move->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;
		}
		if (HasAuthority())
		{
			// El dueño la coloca con su reloj: no se le corrige mientras cuelga.
			Move->bIgnoreClientMovementErrorChecksAndCorrection = true;
		}
		// La zona se actualiza después de su movimiento: la última palabra sobre dónde está la tiene el pico.
		PrimaryActorTick.AddPrerequisite(Move, Move->PrimaryComponentTick);
	}
	// Pataleta en el aire: patalea, da puñetazos y sacude la cabeza.
	if (UTN_TurtleAnimInstance* Anim = Turtle->GetMesh() ? Cast<UTN_TurtleAnimInstance>(Turtle->GetMesh()->GetAnimInstance()) : nullptr)
	{
		Anim->SetCelebration(ETNTurtleCelebration::Tantrum);
	}
	SetTurtleHeld(Turtle, true);
}

void ATN_BeachGullZone::EndHoldLocal()
{
	ATortugaCharacter* Turtle = HeldLocal.Get();
	HeldLocal.Reset();
	if (Turtle)
	{
		if (UCharacterMovementComponent* Move = Turtle->GetCharacterMovement())
		{
			PrimaryActorTick.RemovePrerequisite(Move, Move->PrimaryComponentTick);
			if (HasAuthority())
			{
				Move->bIgnoreClientMovementErrorChecksAndCorrection = false;
			}
			if (bSmoothingSaved)
			{
				Move->NetworkSmoothingMode = static_cast<ENetworkSmoothingMode>(SavedSmoothing);
			}
			// Vuelve a caer por su cuenta (en carrera: con la ronda parada, la congela el GameMode).
			if (Move->MovementMode == MOVE_None && IsRaceLive(this) && !Turtle->IsInShell() && !Turtle->IsKnockedDown() && !Turtle->IsDead())
			{
				Move->SetMovementMode(MOVE_Falling);
			}
		}
		if (UTN_TurtleAnimInstance* Anim = Turtle->GetMesh() ? Cast<UTN_TurtleAnimInstance>(Turtle->GetMesh()->GetAnimInstance()) : nullptr)
		{
			Anim->SetCelebration(ETNTurtleCelebration::None);
		}
		Turtle->SetActorRotation(FRotator(0.f, Turtle->GetActorRotation().Yaw, 0.f));
	}
	bSmoothingSaved = false;
	SetTurtleHeld(Turtle, false);
}

void ATN_BeachGullZone::TickHold()
{
	using namespace TNBeachGull;
	// Quién va en el pico ahora según el ataque replicado (en el servidor, además, hasta que la suelta).
	ATortugaCharacter* Want = nullptr;
	float U = 0.f;
	if (Attack.Kind == 2 && Attack.Result == 1)
	{
		ATortugaCharacter* Victim = Attack.Victim;
		U = static_cast<float>(ServerNow(this) - static_cast<double>(Attack.StartTime)) - StrikeTime;
		if (IsValid(Victim) && !Victim->IsDead() && U >= 0.f && U < CarryTime && !Victim->IsInShell() && !Victim->IsKnockedDown()
			&& !(HasAuthority() && bReleased))
		{
			Want = Victim;
		}
	}
	if (HeldLocal.Get() != Want)
	{
		if (HeldLocal.IsValid())
		{
			EndHoldLocal();
		}
		if (Want)
		{
			BeginHoldLocal(Want);
		}
	}
	if (!Want)
	{
		return;
	}
	// Una corrección de red que llegue tarde podría devolverle el movimiento: mientras cuelga, ninguno.
	if (UCharacterMovementComponent* Move = Want->GetCharacterMovement())
	{
		if (Move->MovementMode != MOVE_None)
		{
			Move->StopMovementImmediately();
			Move->DisableMovement();
		}
	}
	// La espalda de su caparazón en el pico: con malla, se coloca por su hueso de la espalda (la pose de pataleta baja
	// el cuerpo); sin ella (servidor dedicado), por la cápsula.
	FVector Loc = GripPath(U) + CourseBack * ShellBack;
	bool bByBone = false;
	if (bHasScreen)
	{
		const USkeletalMeshComponent* Mesh = Want->GetMesh();
		if (Mesh && !Mesh->IsSimulatingPhysics() && Mesh->GetBoneIndex(SpineBone()) != INDEX_NONE)
		{
			Loc -= Mesh->GetSocketLocation(SpineBone()) - Want->GetActorLocation();
			bByBone = true;
		}
	}
	if (!bByBone)
	{
		Loc -= FVector(0.0, 0.0, GripDropFor(Want));
	}
	Want->SetActorLocationAndRotation(Loc, FRotator(0.f, HeldYaw(), 0.f), false, nullptr, ETeleportType::TeleportPhysics);
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
		bWhistlePlayed = false;
		bReleasePlayed = false;
		bPeckPlayed = false;
		if (Attack.Kind == 3 && bValidBird)
		{
			// Le han dado: plumas por el aire y un graznido que se viene abajo.
			const FVector At = Attack.Aim;
			TNBeachKit::BurstAt(Feathers, At + FVector(0.0, 0.0, 200.0), FVector::UpVector, 16);
			if (Voice)
			{
				Voice->SetWorldLocation(At);
				Voice->Play(ETNBeachSfx::Squawk, Pitch * 0.7f, 1.2f);
			}
			Birds[Attack.Bird].JawOpenLeft = 0.6f;
		}
		else if (Attack.Kind != 0 && Voice && bValidBird)
		{
			Voice->SetWorldLocation(Birds[Attack.Bird].Pos);
			Voice->Play(ETNBeachSfx::Squawk, Pitch, 1.1f);
			Birds[Attack.Bird].JawOpenLeft = 0.35f;
		}
	}
	if (Attack.Result != SeenResult)
	{
		SeenResult = Attack.Result;
		if (Attack.Kind == 2 && bValidBird)
		{
			const ATortugaCharacter* Victim = Attack.Victim;
			if (Voice)
			{
				// Con la tortuga: pico que se cierra de golpe y graznido. Sin ella: graznido de rabia (el picotazo, al llegar).
				Voice->SetWorldLocation(Birds[Attack.Bird].Pos);
				if (Attack.Result == 1)
				{
					Voice->Play(ETNBeachSfx::Clack, 1.6f, 1.1f);
				}
				Voice->Play(ETNBeachSfx::Squawk, Pitch * (Attack.Result == 1 ? 1.15f : 0.85f), 1.2f);
			}
			if (Attack.Result == 1 && Victim)
			{
				const FVector At = Victim->GetActorLocation();
				TNBeachKit::BurstAt(Feathers, At + FVector(0.0, 0.0, 250.0), FVector::UpVector, 18);
				TNBeachKit::BurstAt(SandPuff, At - FVector(0.0, 0.0, 60.0), FVector::UpVector, 12);
				UTN_BeachCameraShake::Kick(this, At, 0.55f, 400.f, 2500.f);
				ShowPop(NSLOCTEXT("TNBeach", "GullGrab", "¡ÑAC!"), FColor(255, 200, 40), At + FVector(0.0, 0.0, 300.0), 150.f);
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
		Voice->Play(ETNBeachSfx::Splat, 1.f, 1.4f);
	}
	TNBeachKit::BurstAt(Droplets, At + FVector(0.0, 0.0, 40.0), FVector::UpVector, 26);
	SpawnSplat(At, nullptr, TNBeachGull::SplatRadius * SizeK / 100.f, 12.f);
	for (ATortugaCharacter* Turtle : Hit)
	{
		if (Turtle)
		{
			SpawnSplat(FVector::ZeroVector, Turtle, 1.3f, TNBeachGull::PoopKnock + 4.f);
			TNBeachKit::BurstAt(Droplets, Turtle->GetActorLocation() + FVector(0.0, 0.0, 120.0), FVector::UpVector, 14);
			UTN_BeachCameraShake::Kick(this, Turtle->GetActorLocation(), 0.4f, 200.f, 1200.f);
		}
	}
	ShowPop(NSLOCTEXT("TNBeach", "GullSplat", "¡PLOF!"), FColor(250, 250, 240), At + FVector(0.0, 0.0, 280.0), Hit.Num() > 0 ? 170.f : 120.f);
}

void ATN_BeachGullZone::SpawnSplat(const FVector& Where, ATortugaCharacter* InTurtle, float InScale, float Life)
{
	UStaticMesh* Mesh = nullptr;
	if (InTurtle)
	{
		Mesh = TNBeachKit::CachedMesh(TEXT("Beach.ShellSplat"), [](TNProcMesh::FTNProcMeshBuffers& M) { TNBeachMeshes::BuildShellSplat(M); });
	}
	else
	{
		Mesh = TNBeachKit::CachedMesh(TEXT("Beach.Splat"), [](TNProcMesh::FTNProcMeshBuffers& M) { TNBeachMeshes::BuildSplat(M, 77u); });
	}
	UStaticMeshComponent* Comp = TNBeachKit::AddPart(this, GetRootComponent(), Mesh, FVector::ZeroVector, false);
	if (!Comp)
	{
		return;
	}
	if (InTurtle)
	{
		// En el caparazón, pegado a su espalda: con el ragdoll del derribo va con el cuerpo.
		TNBeachKit::AttachToTurtleBack(Comp, InTurtle, InScale);
	}
	else
	{
		const float Yaw = 360.f * TNBeachKit::Hash01(static_cast<uint32>(Clock * 1000.f) + static_cast<uint32>(Splats.Num()) * 7u);
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
	FBird& Bird = Birds[Index];
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
	float HeadYaw = 6.f * FMath::Sin(Clock * 0.9f + Bird.Phase);
	float JawTarget = 0.f;
	if (bAttacking && Attack.Kind == 2)
	{
		const float U = Tau - StrikeTime;
		if (Tau < ClimbTime)
		{
			// Sube a colocarse aleteando.
			Flap = 40.f * FMath::Sin(Clock * 2.f * PI * 3.f * Rate);
		}
		else if (Tau < StrikeTime - JawLead)
		{
			// En picado: alas recogidas hacia atrás.
			Sweep = 55.f;
			Flap = 8.f;
			HeadPitch = -20.f;
			Tuck = -85.f;
		}
		else if (Tau < StrikeTime || Attack.Result == 0)
		{
			// Frena: alas abiertas hacia delante, patas por delante y el pico abierto.
			Sweep = -15.f;
			Flap = 28.f * FMath::Sin(Clock * 2.f * PI * 4.f * Rate);
			Tuck = 35.f;
			HeadPitch = StrikeHeadPitch;
			HeadYaw = 0.f;
			JawTarget = 40.f;
		}
		else if (Attack.Result == 1 && U < CarryTime)
		{
			// Con la tortuga en el pico: cabeza gacha (la del agarre), aleteo fuerte al subir y más suave al volar.
			HeadPitch = CarryHeadPitch(U);
			HeadYaw = 0.f;
			JawTarget = 9.f + 3.f * FMath::Sin(Clock * 11.f);
			Flap = U < RiseEnd ? 50.f * FMath::Sin(Clock * 2.f * PI * 3.4f * Rate) : 22.f * FMath::Sin(Clock * 2.f * PI * 2.f * Rate);
			Sweep = -5.f;
			Tuck = 15.f + 10.f * FMath::Sin(Clock * 6.f);
		}
		else if (Attack.Result == 1)
		{
			// La ha soltado: pico abierto un momento y remonta.
			JawTarget = U < CarryTime + 0.5f ? 35.f : 0.f;
			Flap = 40.f * FMath::Sin(Clock * 2.f * PI * 3.f * Rate);
		}
		else if (U < PeckHold)
		{
			// Falla y pica en el sitio: cabeza abajo con dos picotazos, alas abiertas para no caerse y patas delante.
			const float Bob = FMath::Abs(FMath::Sin(FMath::Max(0.f, U - PeckDown) * 14.f));
			HeadPitch = PeckHeadPitch + 25.f * Bob;
			HeadYaw = 0.f;
			JawTarget = 22.f * Bob;
			Flap = 18.f * FMath::Sin(Clock * 2.f * PI * 3.f * Rate);
			Sweep = -15.f;
			Tuck = 30.f;
		}
		else
		{
			Flap = 40.f * FMath::Sin(Clock * 2.f * PI * 3.f * Rate);
		}
	}
	else if (bAttacking && Attack.Kind == 3)
	{
		const float StunFor = FMath::Max(DazeFall, HitStunEndTime - Attack.StartTime);
		if (Tau < DazeFall)
		{
			// Cae dando tumbos, aleteando como puede.
			Flap = 55.f * FMath::Sin(Clock * 2.f * PI * 4.f * Rate);
			Sweep = -10.f;
			HeadPitch = 25.f * FMath::Sin(Clock * 9.f);
			Tuck = 0.f;
			JawTarget = 30.f;
		}
		else if (Tau < StunFor)
		{
			// Sentada en la arena, alas caídas y la cabeza dando vueltas.
			Flap = -30.f + 3.f * FMath::Sin(Clock * 2.f);
			Sweep = 25.f;
			Tuck = 0.f;
			HeadPitch = -15.f + 8.f * FMath::Sin(Clock * 1.7f);
			HeadYaw = 30.f * FMath::Sin(Clock * 2.6f);
			JawTarget = 14.f;
		}
		else
		{
			// Despega aleteando fuerte.
			Flap = 45.f * FMath::Sin(Clock * 2.f * PI * 3.2f * Rate);
			Tuck = -40.f;
		}
	}
	else if (bAttacking && Attack.Kind == 1 && Tau < DropTime + 0.4f)
	{
		HeadPitch = -25.f;
	}
	if (Bird.JawOpenLeft > 0.f)
	{
		Bird.JawOpenLeft -= DeltaSeconds;
		JawTarget = FMath::Max(JawTarget, 28.f);
	}
	Bird.Jaw = FMath::FInterpTo(Bird.Jaw, JawTarget, DeltaSeconds, 18.f);
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
	PosePart(Bird.Head, FRotator(HeadPitch, HeadYaw, 0.f));
	if (Jaws.IsValidIndex(Index) && Jaws[Index])
	{
		TNBeachKit::Pose(Jaws[Index], TNBeachMeshes::BirdGeom(Bird.bPelican).BeakBase, FRotator(-Bird.Jaw, 0.f, 0.f));
	}
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
		const float U = Tau - StrikeTime;
		const bool bCarry = bAttacking && Attack.Kind == 2 && Attack.Result == 1 && U >= 0.f && U < CarryTime;
		const bool bFirst = Bird.Pos.IsZero();
		FVector NewPos;
		if (bCarry)
		{
			// Con la tortuga en el pico: el giro va hacia el del agarre y la raíz se coloca para que el pico esté en su
			// caparazón (la tortuga la coloca TickHold con el mismo camino).
			const FRotator Want = CarryRotation(U);
			const FRotator Shown = FMath::RInterpTo(FRotator(Bird.Pitch, Bird.Yaw, Bird.Bank), Want, Dt, 8.f);
			Bird.Pitch = static_cast<float>(Shown.Pitch);
			Bird.Yaw = static_cast<float>(Shown.Yaw);
			Bird.Bank = static_cast<float>(Shown.Roll);
			NewPos = RootForGrip(Bird, GripPath(U), Shown, CarryHeadPitch(U));
			if (U < CatchBlend)
			{
				NewPos = FMath::Lerp(StrikeRoot(Bird, Now), NewPos, static_cast<double>(Smooth01(U / CatchBlend)));
			}
			const FVector Vel = (NewPos - Bird.Pos) / Dt;
			Bird.Vel = bFirst ? FVector::ZeroVector : FMath::Lerp(Bird.Vel, Vel, static_cast<double>(1.f - FMath::Exp(-Dt / 0.15f)));
			Bird.Pos = NewPos;
		}
		else
		{
			NewPos = bAttacking ? AttackPos(Bird, Now, Tau) : CirclePos(Bird, Now);
			const FVector Vel = (NewPos - Bird.Pos) / Dt;
			Bird.Vel = bFirst ? FVector::ZeroVector : FMath::Lerp(Bird.Vel, Vel, static_cast<double>(1.f - FMath::Exp(-Dt / 0.15f)));
			Bird.Pos = NewPos;
			if (Bird.Vel.SizeSquared2D() > 2500.0)
			{
				const float NewYaw = static_cast<float>(Bird.Vel.Rotation().Yaw);
				const float YawRate = FMath::FindDeltaAngleDegrees(Bird.Yaw, NewYaw) / Dt;
				Bird.Yaw = FMath::FixedTurn(Bird.Yaw, NewYaw, 400.f * Dt);
				Bird.Bank = FMath::FInterpTo(Bird.Bank, FMath::Clamp(YawRate * 0.3f, -40.f, 40.f), Dt, 3.f);
			}
			float Climb = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Bird.Vel.Z, FMath::Max(1.0, Bird.Vel.Size2D()))));
			// Al final del picado frena levantando el morro (el pico llega por delante, abierto); si falla, baja el morro
			// para picar en la arena (como lo cuenta PeckRoot).
			const bool bPecking = bAttacking && Attack.Kind == 2 && Attack.Result == 2 && U >= -0.5f && U < PeckHold;
			if (bAttacking && Attack.Kind == 2 && Tau > StrikeTime - 0.5f)
			{
				const float Brake = Smooth01((Tau - (StrikeTime - 0.5f)) / 0.4f);
				if (Attack.Result != 2)
				{
					Climb = FMath::Lerp(Climb, StrikePitch, Brake);
				}
				else if (bPecking)
				{
					Climb = FMath::Lerp(Climb, PeckPitch, Brake);
				}
			}
			Bird.Pitch = FMath::FInterpTo(Bird.Pitch, FMath::Clamp(Climb, -60.f, 45.f), Dt, bPecking ? 12.f : 5.f);
			if (bAttacking && Attack.Kind == 3)
			{
				// Mareada: da tumbos al caer y se tambalea sentada.
				const float StunFor = FMath::Max(DazeFall, HitStunEndTime - Attack.StartTime);
				if (Tau < DazeFall)
				{
					Bird.Bank = 35.f * FMath::Sin(Clock * 11.f);
				}
				else if (Tau < StunFor)
				{
					Bird.Bank = 8.f * FMath::Sin(Clock * 2.2f);
				}
			}
		}
		if (BirdRoots.IsValidIndex(i) && BirdRoots[i])
		{
			BirdRoots[i]->SetWorldTransform(FTransform(FRotator(Bird.Pitch, Bird.Yaw, Bird.Bank), Bird.Pos, FVector(Bird.Scale)));
		}
		if (bNear || bCarry)
		{
			PoseBird(i, DeltaSeconds, bAttacking, Tau);
		}

		// Sombra en la arena: la de verdad, bajo el cuerpo; cuanto más baja el pájaro, más pequeña, nítida y oscura. En el
		// picado se va a donde va a dar (la tortuga, o donde pica si falla) y se cierra sobre ella: el aviso.
		const TNBeachMeshes::FBirdGeom G = TNBeachMeshes::BirdGeom(Bird.bPelican);
		const FVector BodyAt = Bird.Pos + FRotator(Bird.Pitch, Bird.Yaw, Bird.Bank).RotateVector(G.BodyPivot * Bird.Scale);
		float Mark = 0.f;
		FVector MarkAt = BodyAt;
		if (bAttacking && Attack.Kind == 2 && Attack.Result != 1)
		{
			Mark = Tau < StrikeTime + PeckHold ? Smooth01((Tau - ClimbTime) / 0.4f) : 1.f - Smooth01((Tau - StrikeTime - PeckHold) / 0.4f);
			const ATortugaCharacter* Victim = Attack.Victim;
			MarkAt = Attack.Result == 2 ? FVector(Attack.Hold) : ((Attack.bLocked || !Victim) ? FVector(Attack.Aim) : Victim->GetActorLocation());
		}
		const FVector ShadowXY = FMath::Lerp(BodyAt, MarkAt, static_cast<double>(Mark));
		Bird.ShadowTimer -= DeltaSeconds;
		if (Bird.ShadowTimer <= 0.f)
		{
			Bird.ShadowTimer = Mark > 0.f ? 0.05f : 0.25f;
			Bird.ShadowZ = GroundAt(ShadowXY);
		}
		const float Height = FMath::Max(0.f, static_cast<float>(BodyAt.Z) - Bird.ShadowZ);
		// Suelta: nítida y de unos 4 m a ras de arena; en lo alto, más grande y tenue.
		const float FreeH = FMath::Clamp(Height / 8000.f, 0.f, 1.f);
		float ShadowR = Bird.Span * 0.2f * (0.75f + 0.5f * FreeH);
		float Opacity = FMath::Lerp(0.5f, 0.12f, FreeH);
		float Sharp = 1.f - FreeH;
		if (Mark > 0.f)
		{
			// Aviso del picado: grande y tenue arriba; al llegar, del tamaño de lo que coge, oscura y nítida.
			const float DiveH = FMath::Clamp(Height / DiveStartHeight, 0.f, 1.f);
			ShadowR = FMath::Lerp(ShadowR, FMath::Lerp(GrabRadius * SizeK * 1.15f, Bird.Span * 0.32f, DiveH), Mark);
			Opacity = FMath::Lerp(Opacity, FMath::Lerp(0.72f, 0.16f, DiveH), Mark);
			Sharp = FMath::Lerp(Sharp, 1.f - DiveH, Mark);
		}
		if (Shadows.IsValidIndex(i))
		{
			PlaceBirdShadow(Shadows[i], Bird.ShadowEdge, FVector(ShadowXY.X, ShadowXY.Y, Bird.ShadowZ), bNear ? ShadowR : 0.f, Opacity, Sharp);
		}

		// Picado fallido: el picotazo en la arena (o en la sombrilla), con arena que salta y el golpe.
		if (bAttacking && Attack.Kind == 2 && Attack.Result == 2 && U >= PeckDown && !bPeckPlayed)
		{
			bPeckPlayed = true;
			const FVector PeckAt = Attack.Hold;
			if (bNear)
			{
				TNBeachKit::BurstAt(SandPuff, PeckAt + FVector(0.0, 0.0, 40.0), FVector::UpVector, 8);
				TNBeachKit::BurstAt(PeckSand, PeckAt + FVector(0.0, 0.0, 30.0), FVector::UpVector, 26);
				UTN_BeachCameraShake::Kick(this, PeckAt, 0.3f, 400.f, 2500.f);
				ShowPop(NSLOCTEXT("TNBeach", "GullPeck", "¡PIC!"), FColor(255, 240, 200), PeckAt + FVector(0.0, 0.0, 260.0), 130.f);
				if (Voice)
				{
					Voice->SetWorldLocation(PeckAt);
					Voice->Play(ETNBeachSfx::Slam, 1.8f, 0.8f);
					Voice->Play(ETNBeachSfx::Clack, 2.f, 1.f);
				}
			}
		}

		// Graznidos sueltos (abren el pico), el silbido del picado y la suelta.
		if (Voice && bNear)
		{
			Bird.SquawkTimer -= DeltaSeconds;
			if (Bird.SquawkTimer <= 0.f)
			{
				Bird.SquawkTimer = 5.f + 7.f * TNBeachKit::Hash01(static_cast<uint32>(Clock * 100.f) + static_cast<uint32>(i) * 97u);
				Bird.JawOpenLeft = 0.3f;
				Voice->SetWorldLocation(Bird.Pos);
				Voice->Play(ETNBeachSfx::Squawk, (Bird.bPelican ? 0.55f : 1.f) * (0.9f + 0.2f * TNBeachKit::Hash01(static_cast<uint32>(i) + 5u)), 0.7f);
			}
			if (bAttacking && Attack.Kind == 2 && !bSwoopPlayed && Tau >= ClimbTime)
			{
				bSwoopPlayed = true;
				Voice->SetWorldLocation(Bird.Pos);
				Voice->Play(ETNBeachSfx::Swoop, Bird.bPelican ? 0.7f : 1.f, 1.3f);
			}
			if (bAttacking && Attack.Kind == 2 && Attack.Result == 1 && !bReleasePlayed && U >= CarryTime)
			{
				bReleasePlayed = true;
				Voice->SetWorldLocation(Bird.Pos);
				Voice->Play(ETNBeachSfx::Squawk, Bird.bPelican ? 0.6f : 1.2f, 1.2f);
				TNBeachKit::BurstAt(Feathers, Bird.Pos, FVector::UpVector, 8);
			}
		}
		// Arena que levantan las alas mientras tira de ella cerca del suelo.
		if (bCarry && U < TugTime + 0.3f && bNear)
		{
			DropTrailTimer -= DeltaSeconds;
			if (DropTrailTimer <= 0.f)
			{
				DropTrailTimer = 0.12f;
				TNBeachKit::BurstAt(SandPuff, FVector(Attack.Hold) - FVector(0.0, 0.0, 60.0), FVector::UpVector, 3);
			}
		}
	}

	// Cagada que cae (grande, con su estela) y su sombra que se encoge.
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
			// Sale de debajo de la cola del pájaro y cae acelerando.
			const FVector Top = Target + FVector(0.0, 0.0, PoopHeight + 300.0);
			const FVector At = FMath::Lerp(Top, Target, static_cast<double>(FMath::Pow(U, 1.8f)));
			Dropping->SetWorldTransform(FTransform(FRotator(8.f * FMath::Sin(Clock * 9.f), Clock * 60.f, 0.f), At, FVector(SizeK * DropScale)));
			// Su sombra se encoge, se oscurece y se afila según cae.
			PlaceBirdShadow(DropShadow, DropShadowEdge, Target, FMath::Lerp(520.f, SplatRadius * SizeK, FMath::Pow(U, 1.2f)), FMath::Lerp(0.2f, 0.68f, U), U);
			bShowDrop = true;
			DropTrailTimer -= DeltaSeconds;
			if (DropTrailTimer <= 0.f)
			{
				DropTrailTimer = 0.04f;
				TNBeachKit::BurstAt(Trail, At + FVector(0.0, 0.0, 90.0 * SizeK), FVector::UpVector, 1);
			}
			if (!bWhistlePlayed && Voice)
			{
				// Silbido de lo que cae: aviso para apartarse.
				bWhistlePlayed = true;
				Voice->SetWorldLocation(Target + FVector(0.0, 0.0, 600.0));
				Voice->Play(ETNBeachSfx::Swoop, 1.9f, 0.7f);
			}
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
			// La de la arena es plana (encoge a lo ancho); la del caparazón, entera.
			const bool bFlat = Splat->GetAttachParent() == GetRootComponent();
			Splat->SetWorldScale3D(FVector(K, K, bFlat ? SplatScale[s] : K));
		}
	}

	TNBeachKit::TickEmitterIfBusy(Droplets, DeltaSeconds, View);
	TNBeachKit::TickEmitterIfBusy(Feathers, DeltaSeconds, View);
	TNBeachKit::TickEmitterIfBusy(Trail, DeltaSeconds, View);
	TNBeachKit::TickEmitterIfBusy(SandPuff, DeltaSeconds, View);
	TNBeachKit::TickEmitterIfBusy(PeckSand, DeltaSeconds, View);
}
