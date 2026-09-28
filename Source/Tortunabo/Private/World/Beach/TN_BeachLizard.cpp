#include "World/Beach/TN_BeachLizard.h"
#include "World/Beach/TN_BeachCameraShake.h"
#include "World/Beach/TN_BeachDecorField.h"
#include "World/Beach/TN_BeachRaceGenerator.h"
#include "World/Beach/TN_BeachEnemySynth.h"
#include "World/Beach/TN_BeachLoot.h"
#include "World/Beach/TN_BeachSandWorm.h"
#include "TN_BeachEnemyKit.h"
#include "TN_BeachEnemyMeshes.h"
#include "Components/StaticMeshComponent.h"
#include "Core/TN_InventoryTypes.h"
#include "Core/TN_Log.h"
#include "DrawDebugHelpers.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Player/TN_CarryComponent.h"
#include "Player/TortugaCharacter.h"
#include "World/ProcMap/TN_ProcSearchSpot.h"
#include "World/TN_PickupInteractableBase.h"
#include "World/TN_ScorePickup.h"
#include "World/TN_ScoreShells.h"

namespace TNBeachLizardTuning
{
	/**
	 * Estados del lagarto (Mover.State). Lunge: el mordedor se lanza a por una tortuga; Shake: la tiene en la boca y la
	 * zarandea; Dazed: mareado por algo que se le ha lanzado.
	 */
	enum class EState : uint8 { Bask, Alert, Scare, Flee, HideRock, Burrow, Hidden, Emerge, Walk, Lunge, Shake, Dazed };

	inline uint8 ToByte(EState E)
	{
		return static_cast<uint8>(E);
	}

	/** Reparto de caracteres por semilla: huidizos, generosos y el resto mordedores. */
	constexpr float ShyShare = 0.45f;
	constexpr float GenerousShare = 0.3f;

	/** Radios (cm, desde el centro del lagarto, por el tamaño): alerta, susto o huida, y huida sin pensárselo. */
	constexpr float AlertRadius = 3000.f;
	constexpr float ScareRadius = 1700.f;
	constexpr float PanicRadius = 900.f;
	/** Probabilidad de dar un susto (si no, huye sin más). */
	constexpr float ScareChance = 0.7f;
	/** Toma el sol este rato (s) y luego se va a otro sitio de su zona (hasta este tanto de la huella). */
	constexpr float BaskMin = 4.f;
	constexpr float BaskMax = 8.f;
	constexpr float RoamReach = 0.6f;
	/** Radio del cuerpo en planta (cm, por el tamaño) para apartarse de los demás enemigos. */
	constexpr float BodyRadius = 380.f;
	/** Amago del susto: velocidad y duración; empujón a quien esté cerca de la cabeza (pequeño, sin aturdir). */
	constexpr float LungeSpeed = 700.f;
	constexpr float LungeTime = 0.35f;
	constexpr float ScareTime = 0.9f;
	constexpr float ShoveRadius = 800.f;
	constexpr float ShoveSpeed = 650.f;
	constexpr float ShoveUp = 380.f;
	/** Huida (cm/s: 1,9 veces el esprint de la tortuga), búsqueda de escondite y vuelta a su sitio. */
	constexpr float FleeSpeed = 1500.f;
	constexpr float HideSearch = 4500.f;
	constexpr float WalkSpeed = 380.f;
	/** Tiempos (s): meterse bajo la roca, enterrarse, escondido y salir. */
	constexpr float HideRockTime = 0.5f;
	constexpr float BurrowTime = 1.3f;
	constexpr float HiddenMin = 7.f;
	constexpr float HiddenMax = 12.f;
	constexpr float EmergeTime = 1.f;
	/** No sale si hay una tortuga más cerca que esto. */
	constexpr float EmergeClear = 1800.f;

	// ── Mordedor ──
	/** Se lanza a por una tortuga a esta distancia (cm, por el tamaño), a esta velocidad (más que el esprint) y como mucho tanto rato. */
	constexpr float BiteTrigger = 1500.f;
	constexpr float BiteDashSpeed = 1300.f;
	constexpr float BiteDashTime = 1.3f;
	/** La muerde con la punta del hocico a menos de esto (cm, por el tamaño, más un margen) de su centro. */
	constexpr float BiteReach = 260.f;
	/** Zarandeo: cuánto dura (s), cuánto gira la cabeza de lado a lado (grados), cuántas veces por segundo y con qué cabeceo. */
	constexpr float ShakeTime = 1.3f;
	constexpr float ShakeYaw = 38.f;
	constexpr float ShakeHz = 3.2f;
	constexpr float ShakePitch = 16.f;
	/** Al soltarla: mareada en bola (s), lanzada de lado y hacia arriba (cm/s), y cuánto la deja en paz después (s). */
	constexpr float BiteStun = 1.5f;
	constexpr float TossSpeed = 650.f;
	constexpr float TossUp = 450.f;
	constexpr float BiteIgnore = 10.f;

	// ── Generoso ──
	/** Probabilidad de que el premio sea un objeto (si no, una concha de puntos) y de que la concha sea de 50. */
	constexpr float PrizeItemChance = 0.6f;
	constexpr float PrizeBigShellChance = 0.3f;

	inline float TurnToward(float From, float To, float MaxStep)
	{
		return FMath::UnwindDegrees(From + FMath::Clamp(FMath::FindDeltaAngleDegrees(From, To), -MaxStep, MaxStep));
	}

	/** Elementos de la playa debajo de los que se mete un lagarto. */
	inline bool IsHideElement(ETNBeachElement E)
	{
		switch (E)
		{
		case ETNBeachElement::Rock:
		case ETNBeachElement::RockCluster:
		case ETNBeachElement::MossyLog:
		case ETNBeachElement::Driftwood:
		case ETNBeachElement::ShipSailWreck:
		case ETNBeachElement::OldPlanks:
		case ETNBeachElement::SandCastleSmall:
		case ETNBeachElement::Sandbags:
		case ETNBeachElement::AmmoCrate:
		case ETNBeachElement::CamoNet:
			return true;
		default:
			return false;
		}
	}

	/** Escondido de verdad (bajo una roca o enterrado): no se le ve ni se le puede dar. */
	inline bool IsHidden(EState State)
	{
		return State == EState::HideRock || State == EState::Burrow || State == EState::Hidden;
	}
}

ATN_BeachLizard::ATN_BeachLizard()
{
	NetFrequencyNear = 8.f;
	SetNetUpdateFrequency(NetFrequencyNear);
	bThrottleWhenFar = true;
}

void ATN_BeachLizard::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_BeachLizard, HeldVictim);
}

float ATN_BeachLizard::GetBodyRadius() const
{
	return TNBeachLizardTuning::BodyRadius * SizeK;
}

ETNBeachLizardTemper ATN_BeachLizard::TemperOfSeed(int32 Seed)
{
	const float H = TNBeachKit::Hash01(static_cast<uint32>(Seed) * 2246822519u + 0x2545F491u);
	if (H < TNBeachLizardTuning::ShyShare)
	{
		return ETNBeachLizardTemper::Shy;
	}
	return H < TNBeachLizardTuning::ShyShare + TNBeachLizardTuning::GenerousShare ? ETNBeachLizardTemper::Generous : ETNBeachLizardTemper::Biter;
}

int32 ATN_BeachLizard::FindSeedForTemper(ETNBeachLizardTemper InTemper, int32 Start)
{
	for (int32 k = 0; k < 4096; ++k)
	{
		if (TemperOfSeed(Start + k) == InTemper)
		{
			return Start + k;
		}
	}
	return Start;
}

const TCHAR* ATN_BeachLizard::TemperName(ETNBeachLizardTemper InTemper)
{
	switch (InTemper)
	{
	case ETNBeachLizardTemper::Generous: return TEXT("generoso");
	case ETNBeachLizardTemper::Biter: return TEXT("mordedor");
	case ETNBeachLizardTemper::Shy:
	default: return TEXT("huidizo");
	}
}

void ATN_BeachLizard::ApplySpec()
{
	SizeK = FMath::Clamp(Spec.SizeScale, 0.8f, 1.15f);
	Temper = TemperOfSeed(Spec.Seed);
	GroundZ = static_cast<float>(GetActorLocation().Z);
	BuildLizard();
}

void ATN_BeachLizard::BuildLizard()
{
	if (Scaler)
	{
		return;
	}
	USceneComponent* RigRoot = MakeRig();
	Scaler = NewObject<USceneComponent>(this, TEXT("LizardScaler"));
	Scaler->SetupAttachment(RigRoot);
	Scaler->SetRelativeScale3D(FVector(TNBeach::Scale * SizeK));
	Scaler->RegisterComponent();

	const int32 Pal = ((Spec.Seed % 4) + 4) % 4;
	TNFauna::FTNFaunaQuadLook Look = TNBeachMeshes::LizardLook(Pal);
	// El carácter se ve: el mordedor lleva la cresta roja de púas; el generoso, motas y collar dorados.
	if (Temper == ETNBeachLizardTemper::Biter)
	{
		TNBeachMeshes::MakeLizardBiter(Look);
	}
	TArray<TNFauna::FTNFaunaPart> Parts;
	TNFauna::FTNFaunaRig FaunaRig;
	TNFauna::TNFaunaBuildQuad(Look, true, Parts, FaunaRig);
	RigHeight = static_cast<float>(FaunaRig.Height);
	BodyHalfLen = static_cast<float>(Look.Len);
	BodyHalfWidth = static_cast<float>(Look.Width);
	HeadSize = static_cast<float>(Look.Head);
	TonguePivot = TNBeachMeshes::LizardTonguePivot(Look);
	for (FVector& P : LegPivots)
	{
		P = FVector::ZeroVector;
	}
	// Pivotes en todas las máquinas (la cabeza y la boca sirven al servidor para el empujón y el mordisco).
	for (const TNFauna::FTNFaunaPart& Part : Parts)
	{
		switch (Part.Bone)
		{
		case TNFauna::ETNFaunaBone::Body: BodyPivot = Part.Pivot; break;
		case TNFauna::ETNFaunaBone::Head: HeadPivot = Part.Pivot; break;
		case TNFauna::ETNFaunaBone::Tail: TailPivot = Part.Pivot; break;
		case TNFauna::ETNFaunaBone::LegFL: LegPivots[0] = Part.Pivot; break;
		case TNFauna::ETNFaunaBone::LegFR: LegPivots[1] = Part.Pivot; break;
		case TNFauna::ETNFaunaBone::LegBL: LegPivots[2] = Part.Pivot; break;
		case TNFauna::ETNFaunaBone::LegBR: LegPivots[3] = Part.Pivot; break;
		default: break;
		}
	}
	if (!bHasScreen)
	{
		return;
	}

	// Primero el cuerpo (las demás piezas cuelgan de él).
	const int32 TemperIndex = static_cast<int32>(Temper);
	Legs.SetNum(4);
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (int32 i = 0; i < Parts.Num(); ++i)
		{
			TNFauna::FTNFaunaPart& Part = Parts[i];
			const bool bIsBody = Part.Bone == TNFauna::ETNFaunaBone::Body;
			if ((Pass == 0) != bIsBody)
			{
				continue;
			}
			if (bIsBody && Pal == 2)
			{
				TNBeachMeshes::AddLizardSpots(Part.Mesh, Look);
			}
			if (bIsBody && Temper == ETNBeachLizardTemper::Generous)
			{
				TNBeachMeshes::AddLizardGoldSpots(Part.Mesh, Look);
			}
			const TNProcMesh::FTNProcMeshBuffers& Buffers = Part.Mesh;
			UStaticMesh* Mesh = TNBeachKit::CachedMesh(FString::Printf(TEXT("Beach.Lizard.%d.%d.%d"), Pal, TemperIndex, i),
				[&Buffers](TNProcMesh::FTNProcMeshBuffers& M) { M = Buffers; });
			USceneComponent* Parent = bIsBody ? Scaler.Get() : static_cast<USceneComponent*>(Body.Get());
			UStaticMeshComponent* Comp = TNBeachKit::AddPart(this, Parent, Mesh, Part.Pivot, true);
			switch (Part.Bone)
			{
			case TNFauna::ETNFaunaBone::Body: if (!Body) { Body = Comp; } break;
			case TNFauna::ETNFaunaBone::Head: Head = Comp; break;
			case TNFauna::ETNFaunaBone::Tail: Tail = Comp; break;
			case TNFauna::ETNFaunaBone::LegFL: Legs[0] = Comp; break;
			case TNFauna::ETNFaunaBone::LegFR: Legs[1] = Comp; break;
			case TNFauna::ETNFaunaBone::LegBL: Legs[2] = Comp; break;
			case TNFauna::ETNFaunaBone::LegBR: Legs[3] = Comp; break;
			default: break;
			}
		}
	}
	UStaticMesh* TongueMesh = TNBeachKit::CachedMesh(TEXT("Beach.Lizard.Tongue"), [](TNProcMesh::FTNProcMeshBuffers& M) { TNBeachMeshes::BuildLizardTongue(M); });
	Tongue = TNBeachKit::AddPart(this, Head, TongueMesh, TonguePivot, false);
	if (Tongue)
	{
		Tongue->SetRelativeScale3D(FVector(0.01f, 1.f, 1.f));
	}

	using TNAmbientFX::EShape;
	TNAmbientFX::FEmitterDesc DustDesc = TNBeachKit::MakeDesc(EShape::Puff, FLinearColor(0.86f, 0.77f, 0.58f), true, 0.55f, 36, 0.f, 350.f, -60.f, 0.9f, 1.8f, 120.f, 320.f);
	DustDesc.SpawnRadius = 300.f;
	DustDesc.Spread = 0.9f;
	TNBeachKit::InitEmitter(Dust, this, DustDesc, static_cast<uint32>(Spec.Seed) + 31u);
	if (Temper == ETNBeachLizardTemper::Generous)
	{
		// Destellos dorados sobre el lomo del generoso (y un estallido al dejar el premio).
		TNAmbientFX::FEmitterDesc GlintDesc = TNBeachKit::MakeDesc(EShape::Ember, FLinearColor(1.f, 0.85f, 0.3f), false, 1.f, 30, 0.f, 220.f, -80.f, 0.5f, 1.1f, 40.f, 12.f);
		GlintDesc.Buoyancy = 120.f;
		GlintDesc.Spread = 1.f;
		GlintDesc.SpawnRadius = 220.f;
		TNBeachKit::InitEmitter(Glints, this, GlintDesc, static_cast<uint32>(Spec.Seed) + 32u);
	}
	GetVoice(RigRoot, 1000.f, 8000.f);
	LastShown = ShownLoc;
}

// ─────────────────────────────────────────────────────────────────────────────
// Servidor
// ─────────────────────────────────────────────────────────────────────────────

ATortugaCharacter* ATN_BeachLizard::NearestTurtle(float& OutDist) const
{
	TArray<ATortugaCharacter*> Turtles;
	GatherTurtles(this, Turtles);
	ATortugaCharacter* Best = nullptr;
	OutDist = 1.0e9f;
	for (ATortugaCharacter* Turtle : Turtles)
	{
		// La que tiene en la boca no cuenta (no le va a asustar la que zarandea).
		if (Turtle == HeldVictim)
		{
			continue;
		}
		const float Dist = static_cast<float>(FVector::Dist2D(Turtle->GetActorLocation(), SimLoc));
		if (Dist < OutDist)
		{
			OutDist = Dist;
			Best = Turtle;
		}
	}
	return Best;
}

bool ATN_BeachLizard::FindHideSpot(const FVector& From, FVector& OutSpot) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	FVector ToThreat = From - SimLoc;
	ToThreat.Z = 0.0;
	ToThreat = ToThreat.GetSafeNormal();
	double BestSq = FMath::Square(static_cast<double>(TNBeachLizardTuning::HideSearch));
	bool bFound = false;
	auto Consider = [&](const FVector& At, double Footprint)
	{
		FVector ToSpot = At - SimLoc;
		ToSpot.Z = 0.0;
		const double DistSq = ToSpot.SizeSquared();
		if (DistSq > BestSq)
		{
			return;
		}
		// Nunca hacia la tortuga de la que huye.
		if (DistSq > 1.0 && FVector::DotProduct(ToSpot.GetSafeNormal(), ToThreat) > 0.35)
		{
			return;
		}
		FVector Away = At - From;
		Away.Z = 0.0;
		OutSpot = At + Away.GetSafeNormal() * (Footprint * 0.4);
		BestSq = DistSq;
		bFound = true;
	};
	for (TActorIterator<ATN_BeachElement> It(World); It; ++It)
	{
		const ATN_BeachElement* Element = *It;
		if (!Element || Element == this || !TNBeachLizardTuning::IsHideElement(Element->GetSpec().Element))
		{
			continue;
		}
		Consider(Element->GetActorLocation(), Element->GetFootprintRadius());
	}
	// El decorado de la ronda no es un actor (es local e instanciado, ATN_BeachDecorField): sus piezas, del reparto.
	if (const ATN_BeachRaceGenerator* Gen = FindGenerator())
	{
		const ATN_BeachDecorField* Field = Gen->GetDecorField();
		const TArray<TNBeachLayout::FItem>& Items = Gen->GetRoundLayout().Items;
		const FTransform GenXf = Gen->GetActorTransform();
		for (int32 i = 0; i < Items.Num(); ++i)
		{
			const TNBeachLayout::FItem& Item = Items[i];
			if (!TNBeachLizardTuning::IsHideElement(Item.Element) || (Field && !Field->HasItem(i)))
			{
				continue;
			}
			Consider(GenXf.TransformPosition(FVector(Item.Pos.X, Item.Pos.Y, TNBeachLayout::PlacementZ(Item))), Item.Radius);
		}
	}
	return bFound;
}

bool ATN_BeachLizard::RunToward(const FVector& Goal, float MoveSpeed, float DeltaSeconds, float TurnRate)
{
	FVector Flat = Goal - SimLoc;
	Flat.Z = 0.0;
	const double Dist = Flat.Size();
	if (Dist < 80.0)
	{
		return true;
	}
	const FVector Dir = Flat / Dist;
	FVector Next = SimLoc + Dir * FMath::Min(Dist, static_cast<double>(MoveSpeed * DeltaSeconds));
	// Se aparta de los demás enemigos; lo del reparto no lo rodea (se mete debajo de las rocas y los troncos).
	Next = ResolveStep(Next, GetBodyRadius(), false);
	GroundTimer -= DeltaSeconds;
	if (GroundTimer <= 0.f)
	{
		GroundTimer = 0.1f;
		float Z = GroundZ;
		// La arena (sin trazas): con una traza se subiría encima de la roca en vez de meterse debajo.
		if (GroundHeightAt(Next, Z))
		{
			GroundZ = Z;
		}
	}
	Next.Z = FMath::FInterpTo(SimLoc.Z, static_cast<double>(GroundZ), static_cast<double>(DeltaSeconds), 8.0);
	ServerMoveTo(Next, TNBeachLizardTuning::TurnToward(SimYaw, static_cast<float>(Dir.Rotation().Yaw), TurnRate * DeltaSeconds));
	return false;
}

void ATN_BeachLizard::StartFlee(const FVector& From)
{
	FVector Spot;
	bToRock = FindHideSpot(From, Spot);
	if (!bToRock)
	{
		// Sin escondite a mano: un arreón lejos de la tortuga y se entierra.
		FVector Away = SimLoc - From;
		Away.Z = 0.0;
		Away = Away.IsNearlyZero() ? FRotator(0.f, SimYaw, 0.f).Vector() : Away.GetSafeNormal();
		Spot = SimLoc + Away * 900.0 * SizeK;
	}
	FleeGoal = Spot;
	ServerSetState(TNBeachLizardTuning::ToByte(TNBeachLizardTuning::EState::Flee), Spot);
}

void ATN_BeachLizard::ReactClose(ATortugaCharacter* Near, float Dist)
{
	using TNBeachLizardTuning::EState;
	using TNBeachLizardTuning::ToByte;
	if (!Near)
	{
		return;
	}
	switch (Temper)
	{
	case ETNBeachLizardTemper::Biter:
		// No huye: si se le puede morder, a por ella; si no (en bola, en el suelo, recién mordida), la vigila.
		if (IsRaceLive(this) && CanBite(Near) && Dist < TNBeachLizardTuning::BiteTrigger * SizeK)
		{
			StartLunge(Near);
		}
		else if (static_cast<EState>(GetMoverState()) != EState::Alert)
		{
			Threat = Near;
			CalmTime = 0.f;
			ServerSetState(ToByte(EState::Alert), Near->GetActorLocation());
		}
		break;
	case ETNBeachLizardTemper::Generous:
		// Se va sin asustar a nadie y, la primera vez, deja su premio donde estaba.
		if (!bPrizeDropped)
		{
			DropPrize();
		}
		StartFlee(Near->GetActorLocation());
		break;
	case ETNBeachLizardTemper::Shy:
	default:
		if (Dist < TNBeachLizardTuning::PanicRadius * SizeK || ServerRng.FRand() >= TNBeachLizardTuning::ScareChance)
		{
			StartFlee(Near->GetActorLocation());
		}
		else
		{
			bShoved = false;
			ServerSetState(ToByte(EState::Scare), Near->GetActorLocation());
		}
		break;
	}
}

bool ATN_BeachLizard::CanBite(const ATortugaCharacter* Turtle) const
{
	if (!IsTargetable(Turtle) || Turtle->IsInShell() || ATN_BeachSandWorm::IsBeingEaten(Turtle))
	{
		return false;
	}
	const UTN_CarryComponent* Carry = Turtle->GetCarryComponent();
	return !(Carry && Carry->IsBeingCarried());
}

void ATN_BeachLizard::StartLunge(ATortugaCharacter* Victim)
{
	LungeTarget = Victim;
	Threat = Victim;
	ServerSetState(TNBeachLizardTuning::ToByte(TNBeachLizardTuning::EState::Lunge), Victim->GetActorLocation());
}

void ATN_BeachLizard::StartBite(ATortugaCharacter* Victim)
{
	HeldVictim = Victim;
	IgnoreTurtle(Victim, TNBeachLizardTuning::BiteIgnore);
	// Si llevaba a otra en brazos, la suelta.
	if (UTN_CarryComponent* Carry = Victim->GetCarryComponent())
	{
		if (Carry->IsCarrying())
		{
			Carry->ForceRelease(false);
		}
	}
	ServerSetState(TNBeachLizardTuning::ToByte(TNBeachLizardTuning::EState::Shake), Victim->GetActorLocation());
	ForceNetUpdate();
	// El movimiento se apaga ya en el servidor (los clientes, al recibir el estado).
	TickHold();
	MulticastBite(MouthAt(SimLoc, SimYaw, FRotator(TNBeachLizardTuning::ShakePitch, 0.f, 0.f)), true);
	UE_LOG(LogTortunabo, Log, TEXT("[Playa] %s (mordedor) muerde a %s."), *GetName(), *GetNameSafe(Victim));
}

void ATN_BeachLizard::ReleaseVictim(bool bToss)
{
	ATortugaCharacter* Victim = HeldVictim;
	const FRotator HeadRot = ShakeHeadRotation(GetStateAge());
	const FVector Mouth = MouthAt(SimLoc, SimYaw, HeadRot);
	HeldVictim = nullptr;
	ForceNetUpdate();
	EndHoldTurtle();
	if (!IsValid(Victim) || Victim->IsDead() || !IsRaceLive(this) || ATN_BeachSandWorm::IsBeingEaten(Victim))
	{
		return;
	}
	// La lanza de lado (hacia donde apunta el hocico al soltar), mareada en bola; sin lanzarla, cae con un mareo corto.
	FVector Side = FRotator(0.f, SimYaw + HeadRot.Yaw + (HeadRot.Yaw >= 0.f ? 70.f : -70.f), 0.f).Vector();
	Side.Z = 0.0;
	const FVector Launch = bToss ? Side * TNBeachLizardTuning::TossSpeed + FVector(0.0, 0.0, TNBeachLizardTuning::TossUp) : FVector(0.0, 0.0, 100.0);
	StunTurtle(Victim, bToss ? TNBeachLizardTuning::BiteStun : 1.f, Launch);
	if (bToss)
	{
		MulticastBite(Mouth, false);
	}
}

void ATN_BeachLizard::DropPrize()
{
	bPrizeDropped = true;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FVector Where = SimLoc;
	float Z = static_cast<float>(Where.Z);
	if (GroundHeightAt(Where, Z))
	{
		Where.Z = Z;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Un objeto del catálogo con los pesos de la carrera (como el botín suelto) o, si no, una concha de puntos.
	if (ServerRng.FRand() < TNBeachLizardTuning::PrizeItemChance)
	{
		const UDataTable* Catalog = LoadObject<UDataTable>(nullptr, TNBeachLoot::CatalogPath(), nullptr, LOAD_NoWarn);
		FTN_InventoryItem Picked;
		if (Catalog && ATN_ProcSearchSpot::PickCatalogItem(Catalog,
			[](FName RowName, const FTN_InventoryItem& Row) { return TNBeachLoot::RaceWeight(RowName, Row); }, Picked) && Picked.PickupActorClass)
		{
			if (ATN_PickupInteractableBase* Pickup = World->SpawnActor<ATN_PickupInteractableBase>(Picked.PickupActorClass, Where + FVector(0.0, 0.0, 3.0),
				FRotator(0.0, ServerRng.FRandRange(0.f, 360.f), 0.0), Params))
			{
				Pickup->InitializeFromInventoryItem(Picked);
				Prize = Pickup;
			}
		}
	}
	if (!Prize.IsValid())
	{
		UClass* ShellClass = LoadClass<ATN_ScorePickup>(nullptr, TEXT("/Game/Blueprints/Gameplay/Items/BP_ScorePickup.BP_ScorePickup_C"));
		if (!ShellClass)
		{
			ShellClass = ATN_ScorePickup::StaticClass();
		}
		const FTransform ShellAt(FRotator(0.0, GetActorRotation().Yaw, 0.0), Where + FVector(0.0, 0.0, TNScoreShells::Hover));
		if (ATN_ScorePickup* Shell = World->SpawnActorDeferred<ATN_ScorePickup>(ShellClass, ShellAt, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Shell->SetScoreValue(ServerRng.FRand() < TNBeachLizardTuning::PrizeBigShellChance ? 50 : 25);
			Shell->FinishSpawning(ShellAt);
			Prize = Shell;
		}
	}
	MulticastPrize(Where);
	UE_LOG(LogTortunabo, Log, TEXT("[Playa] %s (generoso) deja %s."), *GetName(), *GetNameSafe(Prize.Get()));
}

void ATN_BeachLizard::ServerTick(float DeltaSeconds)
{
	using TNBeachLizardTuning::EState;
	using TNBeachLizardTuning::ToByte;
	StateLeft -= DeltaSeconds;
	const EState State = static_cast<EState>(GetMoverState());
	const float Age = GetStateAge();

	// Mareado por lo que se le ha lanzado: tumbado con pajaritos (si tenía a alguien en la boca, ya lo ha soltado).
	if (IsHitStunned())
	{
		if (State != EState::Dazed)
		{
			if (HeldVictim)
			{
				ReleaseVictim(false);
			}
			ServerSetState(ToByte(EState::Dazed));
		}
		return;
	}

	float Dist = 1.0e9f;
	ATortugaCharacter* Near = NearestTurtle(Dist);
	const float Alert = TNBeachLizardTuning::AlertRadius * SizeK;
	const float Scare = TNBeachLizardTuning::ScareRadius * SizeK;

	switch (State)
	{
	case EState::Bask:
		if (Near && Temper == ETNBeachLizardTemper::Biter && Dist < TNBeachLizardTuning::BiteTrigger * SizeK && CanBite(Near) && IsRaceLive(this))
		{
			StartLunge(Near);
		}
		else if (Near && Dist < Alert)
		{
			Threat = Near;
			CalmTime = 0.f;
			ServerSetState(ToByte(EState::Alert), Near->GetActorLocation());
		}
		else if (StateLeft <= 0.f)
		{
			// Tras un rato al sol, se va a otro sitio de su zona (no dentro de una roca).
			FVector Goal = Home;
			for (int32 Try = 0; Try < 5; ++Try)
			{
				const float Angle = ServerRng.FRandRange(0.f, 2.f * PI);
				const float Roam = GetFootprintRadius() * TNBeachLizardTuning::RoamReach * FMath::Sqrt(ServerRng.FRandRange(0.25f, 1.f));
				const FVector Candidate = Home + FVector(FMath::Cos(Angle) * Roam, FMath::Sin(Angle) * Roam, 0.f);
				if (!IsInsideObstacle(Candidate, GetBodyRadius()))
				{
					Goal = Candidate;
					break;
				}
			}
			ServerSetState(ToByte(EState::Walk), Goal);
		}
		break;

	case EState::Alert:
	{
		if (Near)
		{
			Threat = Near;
			ServerSetAim(Near->GetActorLocation());
			const FVector To = Near->GetActorLocation() - SimLoc;
			ServerMoveTo(SimLoc, TNBeachLizardTuning::TurnToward(SimYaw, static_cast<float>(To.Rotation().Yaw), 90.f * DeltaSeconds));
		}
		if (!Near || Dist > Alert * 1.15f)
		{
			CalmTime += DeltaSeconds;
			if (CalmTime > 2.f)
			{
				StateLeft = ServerRng.FRandRange(TNBeachLizardTuning::BaskMin, TNBeachLizardTuning::BaskMax);
				ServerSetState(ToByte(EState::Bask));
			}
		}
		else if (Dist < Scare || (Temper == ETNBeachLizardTemper::Biter && Dist < TNBeachLizardTuning::BiteTrigger * SizeK))
		{
			ReactClose(Near, Dist);
		}
		else
		{
			CalmTime = 0.f;
		}
		break;
	}

	case EState::Scare:
	{
		// Amago hacia la tortuga, sacudida y bufido; a quien esté pegado a la cabeza, un empujoncito.
		const FVector Aim = Mover.Aim;
		FVector To = Aim - SimLoc;
		To.Z = 0.0;
		const float Facing = static_cast<float>(To.Rotation().Yaw);
		if (Age < TNBeachLizardTuning::LungeTime)
		{
			const FVector Next = SimLoc + To.GetSafeNormal() * (TNBeachLizardTuning::LungeSpeed * DeltaSeconds);
			ServerMoveTo(Next, TNBeachLizardTuning::TurnToward(SimYaw, Facing, 400.f * DeltaSeconds));
		}
		if (!bShoved && Age >= 0.3f && IsRaceLive(this))
		{
			bShoved = true;
			const FVector HeadAt = SimLoc + FRotator(0.f, SimYaw, 0.f).Vector() * ((HeadPivot.X + BodyPivot.X) * TNBeach::Scale * SizeK);
			TArray<ATortugaCharacter*> Turtles;
			GatherTurtles(this, Turtles);
			for (ATortugaCharacter* Turtle : Turtles)
			{
				const FVector At = Turtle->GetActorLocation();
				// Ni a las derribadas, aturdidas o en el pico de una gaviota (su movimiento no es suyo).
				if (!CanBeHit(Turtle) || FVector::Dist2D(At, HeadAt) > TNBeachLizardTuning::ShoveRadius * SizeK)
				{
					continue;
				}
				FVector Away = At - HeadAt;
				Away.Z = 0.0;
				Away = Away.IsNearlyZero() ? FRotator(0.f, SimYaw, 0.f).Vector() : Away.GetSafeNormal();
				const FVector Push = Away * TNBeachLizardTuning::ShoveSpeed + FVector(0.0, 0.0, TNBeachLizardTuning::ShoveUp);
				Turtle->LaunchCharacter(Push, true, true);
				MulticastShove(Turtle, Push);
			}
		}
		if (Age >= TNBeachLizardTuning::ScareTime)
		{
			StartFlee(Aim);
		}
		break;
	}

	case EState::Lunge:
	{
		// El mordedor se lanza a por ella con la boca abierta; si la alcanza de pie, la muerde.
		ATortugaCharacter* Victim = LungeTarget.Get();
		if (!IsRaceLive(this) || !Victim || !CanBite(Victim) || Age > TNBeachLizardTuning::BiteDashTime)
		{
			// Ha fallado: bufa y vuelve a su sitio (el mordedor no se esconde); a esa la deja un momento.
			if (Victim)
			{
				IgnoreTurtle(Victim, 4.f);
			}
			LungeTarget.Reset();
			MulticastBite(MouthAt(SimLoc, SimYaw, FRotator(8.f, 0.f, 0.f)), false);
			ServerSetState(ToByte(EState::Walk), Home);
			break;
		}
		const FVector At = Victim->GetActorLocation();
		ServerSetAim(At);
		RunToward(At, TNBeachLizardTuning::BiteDashSpeed * SizeK, DeltaSeconds, 700.f);
		const FVector Mouth = MouthAt(SimLoc, SimYaw, FRotator(8.f, 0.f, 0.f));
		if (FVector::Dist2D(Mouth, At) < TNBeachLizardTuning::BiteReach * SizeK + 60.f && FMath::Abs(At.Z - Mouth.Z) < 320.0)
		{
			LungeTarget.Reset();
			StartBite(Victim);
		}
		break;
	}

	case EState::Shake:
	{
		// La zarandea en el sitio y la lanza mareada.
		ATortugaCharacter* Victim = HeldVictim;
		const bool bGone = !IsValid(Victim) || Victim->IsDead() || ATN_BeachSandWorm::IsBeingEaten(Victim);
		if (bGone || !IsRaceLive(this))
		{
			ReleaseVictim(false);
			ServerSetState(ToByte(EState::Walk), Home);
		}
		else if (Age >= TNBeachLizardTuning::ShakeTime)
		{
			ReleaseVictim(true);
			StateLeft = ServerRng.FRandRange(TNBeachLizardTuning::BaskMin, TNBeachLizardTuning::BaskMax);
			ServerSetState(ToByte(EState::Walk), Home);
		}
		break;
	}

	case EState::Dazed:
		// Se le ha pasado el mareo: el huidizo y el generoso, a esconderse; el mordedor, a su sitio.
		if (Temper != ETNBeachLizardTemper::Biter && Near)
		{
			StartFlee(Near->GetActorLocation());
		}
		else
		{
			ServerSetState(ToByte(EState::Walk), Home);
		}
		break;

	case EState::Flee:
		if (RunToward(FleeGoal, TNBeachLizardTuning::FleeSpeed * SizeK, DeltaSeconds, 600.f) || Age > 6.f)
		{
			ServerSetState(ToByte(bToRock ? EState::HideRock : EState::Burrow), FleeGoal);
		}
		break;

	case EState::HideRock:
	case EState::Burrow:
		if (Age >= (State == EState::HideRock ? TNBeachLizardTuning::HideRockTime : TNBeachLizardTuning::BurrowTime))
		{
			StateLeft = ServerRng.FRandRange(TNBeachLizardTuning::HiddenMin, TNBeachLizardTuning::HiddenMax);
			ServerSetState(ToByte(EState::Hidden));
		}
		break;

	case EState::Hidden:
		if (StateLeft <= 0.f && (!Near || Dist > TNBeachLizardTuning::EmergeClear * SizeK))
		{
			ServerSetState(ToByte(EState::Emerge));
		}
		break;

	case EState::Emerge:
		if (Age >= TNBeachLizardTuning::EmergeTime)
		{
			StateLeft = ServerRng.FRandRange(TNBeachLizardTuning::BaskMin, TNBeachLizardTuning::BaskMax);
			ServerSetState(ToByte(FVector::Dist2D(SimLoc, Home) > 300.0 ? EState::Walk : EState::Bask), Home);
		}
		break;

	case EState::Walk:
	default:
		// Anda a su sitio o al siguiente rincón al sol (Mover.Aim); si una tortuga se acerca, reacciona o se para a mirarla.
		if (Near && (Dist < Scare || (Temper == ETNBeachLizardTemper::Biter && Dist < TNBeachLizardTuning::BiteTrigger * SizeK)))
		{
			if (Temper == ETNBeachLizardTemper::Biter && !CanBite(Near))
			{
				// El mordedor no huye de la que no puede morder: sigue a lo suyo.
				if (RunToward(FVector(Mover.Aim), TNBeachLizardTuning::WalkSpeed * SizeK, DeltaSeconds, 200.f) || Age > 10.f)
				{
					StateLeft = ServerRng.FRandRange(TNBeachLizardTuning::BaskMin, TNBeachLizardTuning::BaskMax);
					ServerSetState(ToByte(EState::Bask));
				}
			}
			else
			{
				ReactClose(Near, Dist);
			}
		}
		else if (Near && Dist < Alert && (Temper != ETNBeachLizardTemper::Biter || CanBite(Near)))
		{
			Threat = Near;
			CalmTime = 0.f;
			ServerSetState(ToByte(EState::Alert), Near->GetActorLocation());
		}
		else if (RunToward(FVector(Mover.Aim), TNBeachLizardTuning::WalkSpeed * SizeK, DeltaSeconds, 200.f) || Age > 10.f)
		{
			StateLeft = ServerRng.FRandRange(TNBeachLizardTuning::BaskMin, TNBeachLizardTuning::BaskMax);
			ServerSetState(ToByte(EState::Bask));
		}
		break;
	}

	if (IsDebugDraw())
	{
		UWorld* World = GetWorld();
		const FColor TemperColor = Temper == ETNBeachLizardTemper::Biter ? FColor::Red : (Temper == ETNBeachLizardTemper::Generous ? FColor::Yellow : FColor::Green);
		DrawDebugCircle(World, SimLoc + FVector(0.0, 0.0, 30.0), Alert, 40, FColor::Yellow, false, -1.f, 0, 8.f, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), false);
		DrawDebugCircle(World, SimLoc + FVector(0.0, 0.0, 30.0), Scare, 40, FColor::Orange, false, -1.f, 0, 8.f, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), false);
		DrawDebugCircle(World, SimLoc + FVector(0.0, 0.0, 30.0), TNBeachLizardTuning::PanicRadius * SizeK, 32, FColor::Red, false, -1.f, 0, 8.f, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), false);
		DrawDebugString(World, SimLoc + FVector(0.0, 0.0, 1100.0), TemperName(Temper), nullptr, TemperColor, 0.f, true);
		if (State == EState::Flee)
		{
			DrawDebugLine(World, SimLoc + FVector(0.0, 0.0, 200.0), FleeGoal + FVector(0.0, 0.0, 200.0), bToRock ? FColor::Green : FColor::Blue, false, -1.f, 0, 10.f);
		}
	}
}

void ATN_BeachLizard::MulticastShove_Implementation(ATortugaCharacter* Victim, FVector_NetQuantize10 Push)
{
	// El dueño aplica el mismo empujón que el servidor: sin corrección de movimiento.
	if (Victim && !HasAuthority() && Victim->IsLocallyControlled())
	{
		Victim->LaunchCharacter(Push, true, true);
	}
	if (bHasScreen && Victim)
	{
		UTN_BeachCameraShake::Kick(this, Victim->GetActorLocation(), 0.25f, 300.f, 1500.f);
	}
}

void ATN_BeachLizard::MulticastBite_Implementation(FVector_NetQuantize Where, bool bCaught)
{
	if (!bHasScreen)
	{
		return;
	}
	const FVector At = Where;
	if (Voice)
	{
		// Mandíbulas que se cierran de golpe y el bufido.
		Voice->SetWorldLocation(At);
		Voice->Play(ETNBeachSfx::Clack, 0.55f / FMath::Sqrt(SizeK), 1.4f);
		Voice->Play(ETNBeachSfx::Hiss, bCaught ? 0.75f : 0.95f, bCaught ? 1.2f : 0.9f);
	}
	TNBeachKit::BurstAt(Dust, At - FVector(0.0, 0.0, 80.0), FVector::UpVector, bCaught ? 8 : 4);
	if (bCaught)
	{
		UTN_BeachCameraShake::Kick(this, At, 0.45f, 400.f, 2500.f);
		ShowPop(NSLOCTEXT("TNBeach", "LizardBite", "¡ÑAM!"), FColor(240, 70, 60), At + FVector(0.0, 0.0, 280.0), 150.f);
	}
}

void ATN_BeachLizard::MulticastPrize_Implementation(FVector_NetQuantize Where)
{
	if (!bHasScreen)
	{
		return;
	}
	const FVector At = Where;
	if (Voice)
	{
		Voice->SetWorldLocation(At);
		Voice->Play(ETNBeachSfx::Clack, 1.8f, 0.7f);
	}
	TNBeachKit::BurstAt(Glints, At + FVector(0.0, 0.0, 120.0), FVector::UpVector, 20);
	TNBeachKit::BurstAt(Dust, At, FVector::UpVector, 6);
	ShowPop(NSLOCTEXT("TNBeach", "LizardPrize", "¡UN REGALO!"), FColor(255, 215, 60), At + FVector(0.0, 0.0, 260.0), 140.f);
}

void ATN_BeachLizard::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		if (HeldVictim && EndPlayReason == EEndPlayReason::Destroyed)
		{
			ReleaseVictim(false);
		}
		// El premio que nadie ha cogido se va con él (la ronda siguiente reparte otro).
		if (AActor* Left = Prize.Get())
		{
			Left->Destroy();
		}
		Prize.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

// ─────────────────────────────────────────────────────────────────────────────
// Mareo por lo que se le lanza y la tortuga en la boca
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachLizard::ApplyHitStun(float Seconds, AActor* InstigatorActor)
{
	Super::ApplyHitStun(Seconds, InstigatorActor);
	if (!HasAuthority() || !IsHitStunned())
	{
		return;
	}
	if (HeldVictim)
	{
		ReleaseVictim(false);
	}
	if (static_cast<TNBeachLizardTuning::EState>(GetMoverState()) != TNBeachLizardTuning::EState::Dazed)
	{
		LungeTarget.Reset();
		ServerSetState(TNBeachLizardTuning::ToByte(TNBeachLizardTuning::EState::Dazed));
	}
}

bool ATN_BeachLizard::GetHitCapsule(FVector& OutA, FVector& OutB, float& OutRadius) const
{
	if (TNBeachLizardTuning::IsHidden(static_cast<TNBeachLizardTuning::EState>(GetMoverState())))
	{
		return false;
	}
	// Del cuello a las caderas (la cola, fina, no cuenta).
	const double S = TNBeach::Scale * SizeK;
	const FRotator Rot(0.f, ShownYaw, 0.f);
	OutA = ShownLoc + Rot.RotateVector((BodyPivot + HeadPivot) * S);
	OutB = ShownLoc + Rot.RotateVector((BodyPivot - FVector(BodyHalfLen * 0.85, 0.0, 0.0)) * S);
	OutRadius = static_cast<float>(BodyHalfWidth * S * 1.1);
	return true;
}

FVector ATN_BeachLizard::GetHitStunAnchor() const
{
	const double S = TNBeach::Scale * SizeK;
	return ShownLoc + FRotator(0.f, ShownYaw, 0.f).RotateVector((BodyPivot + HeadPivot) * S) + FVector(0.0, 0.0, HeadSize * S * 1.6);
}

float ATN_BeachLizard::GetHitStunScale() const
{
	return 4.5f * SizeK;
}

FRotator ATN_BeachLizard::ShakeHeadRotation(float Age) const
{
	// Cabeza alta, de lado a lado cada vez más y, al final, más despacio (entra y sale en 0,15 s).
	const float In = FMath::Clamp(Age / 0.15f, 0.f, 1.f);
	const float Out = FMath::Clamp((TNBeachLizardTuning::ShakeTime - Age) / 0.15f, 0.f, 1.f);
	const float Envelope = FMath::Min(In, Out);
	const float Yaw = TNBeachLizardTuning::ShakeYaw * Envelope * FMath::Sin(2.f * PI * TNBeachLizardTuning::ShakeHz * Age);
	const float Pitch = TNBeachLizardTuning::ShakePitch * In + 5.f * Envelope * FMath::Sin(Age * 23.f);
	return FRotator(Pitch, Yaw, 0.f);
}

FVector ATN_BeachLizard::MouthAt(const FVector& Base, float Yaw, const FRotator& HeadRot) const
{
	const double S = TNBeach::Scale * SizeK;
	const FVector Local = BodyPivot + HeadPivot + HeadRot.RotateVector(TonguePivot);
	return Base + FRotator(0.f, Yaw, 0.f).RotateVector(Local * S);
}

void ATN_BeachLizard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Después de moverse el lagarto: la tortuga mordida, en su boca.
	TickHold();
}

void ATN_BeachLizard::TickHold()
{
	ATortugaCharacter* Want = nullptr;
	if (static_cast<TNBeachLizardTuning::EState>(GetMoverState()) == TNBeachLizardTuning::EState::Shake && IsValid(HeldVictim) && !HeldVictim->IsDead())
	{
		Want = HeldVictim;
	}
	if (GetHeldTurtle() != Want)
	{
		if (GetHeldTurtle())
		{
			EndHoldTurtle();
		}
		if (Want)
		{
			BeginHoldTurtle(Want);
		}
	}
	if (!Want)
	{
		return;
	}
	// Por el caparazón, atravesada en la boca y zarandeada con la cabeza (las mismas cuentas en todas las máquinas).
	const FRotator HeadRot = ShakeHeadRotation(GetStateAge());
	const FVector Mouth = MouthAt(ShownLoc, ShownYaw, HeadRot);
	PlaceHeldTurtle(Mouth - FVector(0.0, 0.0, 25.0), ShownYaw + HeadRot.Yaw + 90.f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Visual
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachLizard::OnMoverStateChanged(uint8 OldState)
{
	if (!bHasScreen || !Voice)
	{
		return;
	}
	using TNBeachLizardTuning::EState;
	switch (static_cast<EState>(GetMoverState()))
	{
	case EState::Scare:
		Voice->Play(ETNBeachSfx::Hiss, 0.85f / FMath::Sqrt(SizeK), 1.3f);
		break;
	case EState::Lunge:
		// Bufido grave y las patas arañando la arena.
		Voice->Play(ETNBeachSfx::Hiss, 0.6f / FMath::Sqrt(SizeK), 1.4f);
		Voice->Play(ETNBeachSfx::Skitter, 0.6f, 0.9f);
		break;
	case EState::Burrow:
	case EState::Emerge:
		Voice->Play(ETNBeachSfx::Roll, 0.6f, 0.9f);
		break;
	default:
		break;
	}
}

void ATN_BeachLizard::VisualTick(float DeltaSeconds)
{
	using TNBeachLizardTuning::EState;
	Clock += DeltaSeconds;
	FVector Delta = ShownLoc - LastShown;
	Delta.Z = 0.0;
	LastShown = ShownLoc;
	const float Step = static_cast<float>(Delta.Size());
	Moving = FMath::FInterpTo(Moving, FMath::Clamp(Step / FMath::Max(DeltaSeconds, 1e-3f) / 600.f, 0.f, 1.6f), DeltaSeconds, 8.f);
	Gait += Step / (260.f * SizeK);

	const EState State = static_cast<EState>(GetMoverState());
	const float Age = GetStateAge();
	float WantSink = 0.f;
	switch (State)
	{
	case EState::HideRock: WantSink = FMath::Clamp(Age / TNBeachLizardTuning::HideRockTime, 0.f, 1.f); break;
	case EState::Burrow: WantSink = FMath::Clamp((Age - 0.3f) / (TNBeachLizardTuning::BurrowTime - 0.3f), 0.f, 1.f); break;
	case EState::Hidden: WantSink = 1.f; break;
	case EState::Emerge: WantSink = 1.f - FMath::Clamp(Age / TNBeachLizardTuning::EmergeTime, 0.f, 1.f); break;
	case EState::Dazed: WantSink = 0.12f; break;
	default: break;
	}
	Sink = FMath::FInterpTo(Sink, WantSink, DeltaSeconds, 10.f);

	FVector View = ShownLoc;
	TNBeachKit::LocalCamera(GetWorld(), View);
	const bool bNear = ViewDistance < GetVisualRange();
	if (Scaler)
	{
		const bool bVisible = Sink < 0.98f;
		if (Scaler->IsVisible() != bVisible)
		{
			Scaler->SetVisibility(bVisible, true);
		}
		// Mordiendo, sin hundirse (la boca tiene que estar donde la cuentan todas las máquinas).
		const float ShownSink = State == EState::Shake ? 0.f : Sink;
		Scaler->SetRelativeLocation(FVector(0.0, 0.0, -ShownSink * RigHeight * 1.3f * TNBeach::Scale * SizeK));
	}
	if (bNear && Sink < 0.98f)
	{
		PoseLizard(DeltaSeconds);
	}

	// Polvo al enterrarse, al salir y al lanzarse a morder.
	const bool bDigging = State == EState::Burrow || State == EState::Emerge || (State == EState::HideRock && Age < 0.6f) || State == EState::Lunge;
	Dust.Origin = ShownLoc + FVector(0.0, 0.0, 60.0);
	Dust.Desc.Direction = FVector::UpVector;
	Dust.RateScale = bNear && bDigging ? 14.f : (bNear ? FMath::Clamp(Moving - 0.8f, 0.f, 1.f) * 6.f : 0.f);
	TNBeachKit::TickEmitterIfBusy(Dust, DeltaSeconds, View);

	// El generoso destella de vez en cuando (se sabe que lleva algo).
	if (Glints.ISM.IsValid())
	{
		GlintTimer -= DeltaSeconds;
		if (GlintTimer <= 0.f && bNear && Sink < 0.5f && !bPrizeDropped)
		{
			GlintTimer = 0.9f + 0.8f * TNBeachKit::Hash01(static_cast<uint32>(Clock * 10.f) + static_cast<uint32>(Spec.Seed));
			const double S = TNBeach::Scale * SizeK;
			const FVector Back = ShownLoc + FVector(0.0, 0.0, (BodyPivot.Z + 3.5) * S);
			TNBeachKit::BurstAt(Glints, Back, FVector::UpVector, 3);
		}
		TNBeachKit::TickEmitterIfBusy(Glints, DeltaSeconds, View);
	}

	if (Voice && bNear && Moving > 0.5f)
	{
		SkitterTimer -= DeltaSeconds;
		if (SkitterTimer <= 0.f)
		{
			SkitterTimer = 0.3f;
			Voice->Play(ETNBeachSfx::Skitter, 0.65f, 0.35f);
		}
	}
}

void ATN_BeachLizard::PoseLizard(float DeltaSeconds)
{
	using TNBeachLizardTuning::EState;
	const EState State = static_cast<EState>(GetMoverState());
	const float T = Clock;
	const float Age = GetStateAge();
	const float Phase = Gait * 2.f * PI;
	const bool bScare = State == EState::Scare;
	const bool bLunge = State == EState::Lunge;
	const bool bShake = State == EState::Shake;
	const bool bDazed = State == EState::Dazed;
	const bool bAlert = State == EState::Alert || bScare || bLunge;
	const bool bDig = State == EState::Burrow && Age < TNBeachLizardTuning::BurrowTime;

	// Flexiones de vez en cuando al tomar el sol (tres seguidas cada ~7 s).
	float PushUp = 0.f;
	if (State == EState::Bask)
	{
		const float Cycle = FMath::Fmod(T + static_cast<float>(Spec.Seed % 97), 7.f);
		if (Cycle < 1.8f)
		{
			PushUp = 1.3f * FMath::Max(0.f, FMath::Sin(Cycle * 3.f * PI / 1.8f));
		}
	}

	// Cuerpo: culebreo al correr, se hincha y tiembla en el susto, se sacude al enterrarse. Mordiendo, quieto (la boca va
	// donde la cuentan todas las máquinas); mareado, tumbado de lado.
	const float Wiggle = (bShake || bDazed) ? 0.f : 12.f * FMath::Min(1.f, Moving) * FMath::Sin(Phase);
	float Roll = 0.f;
	if (bScare)
	{
		Roll += 6.f * FMath::Sin(T * 40.f);
	}
	if (bDig)
	{
		Roll += 9.f * FMath::Sin(T * 32.f);
	}
	if (bDazed)
	{
		Roll += 14.f + 4.f * FMath::Sin(T * 2.3f);
	}
	const float Puff = bScare ? 1.18f : (bShake ? 1.f : 1.f + 0.02f * FMath::Sin(T * 1.6f));
	const float BodyPitch = bShake ? 0.f : (bLunge ? -3.f : (bAlert ? 4.f : 0.f));
	TNBeachKit::Pose(Body, BodyPivot + FVector(0.0, 0.0, PushUp), FRotator(BodyPitch, Wiggle, bShake ? 0.f : Roll), FVector(1.f, Puff, Puff));

	// Cabeza: cabeceo tranquilo; alerta, mira a la tortuga con la cabeza alta; al lanzarse, estirada; mordiendo, zarandea.
	float HeadPitch = 5.f * FMath::Sin(T * 0.8f);
	float HeadYaw = -Wiggle * 0.6f;
	if (bAlert)
	{
		HeadPitch = bScare ? 24.f + 4.f * FMath::Sin(T * 30.f) : (bLunge ? 8.f : 14.f);
		const FVector ToAim = FVector(Mover.Aim) - ShownLoc;
		HeadYaw = FMath::Clamp(FMath::FindDeltaAngleDegrees(ShownYaw, static_cast<float>(ToAim.Rotation().Yaw)), -50.f, 50.f);
		if (bLunge)
		{
			HeadYaw *= 0.3f;
		}
	}
	if (bShake)
	{
		const FRotator Shake = ShakeHeadRotation(Age);
		HeadPitch = static_cast<float>(Shake.Pitch);
		HeadYaw = static_cast<float>(Shake.Yaw);
	}
	if (bDazed)
	{
		HeadPitch = -12.f + 4.f * FMath::Sin(T * 1.7f);
		HeadYaw = 20.f * FMath::Sin(T * 2.6f);
	}
	TNBeachKit::Pose(Head, HeadPivot, FRotator(HeadPitch, HeadYaw, bDazed ? 12.f : 0.f));

	// Patas en diagonal (la delantera izquierda con la trasera derecha); en las flexiones, estiradas; mordiendo, clavadas
	// y abiertas; mareado, despatarrado.
	for (int32 k = 0; k < Legs.Num(); ++k)
	{
		const bool bFront = k < 2;
		const float Side = (k % 2 == 0) ? -1.f : 1.f;
		const bool bPairA = (k == 0 || k == 3);
		const float LegPhase = Phase + (bPairA ? 0.f : PI);
		float Swing = 28.f * FMath::Min(1.f, Moving) * FMath::Sin(LegPhase) * (bFront ? 1.f : -1.f);
		float Lift = 18.f * FMath::Min(1.f, Moving) * FMath::Max(0.f, FMath::Cos(LegPhase)) + (bDig && bFront ? 25.f * FMath::Abs(FMath::Sin(T * 20.f)) : 0.f);
		const float Press = -PushUp * 14.f;
		if (bShake)
		{
			Swing = (bFront ? -12.f : 12.f) + 4.f * FMath::Sin(T * 17.f + k);
			Lift = -6.f;
		}
		if (bDazed)
		{
			Swing = (bFront ? 25.f : -25.f);
			Lift = -10.f + 6.f * FMath::Sin(T * 3.f + k);
		}
		TNBeachKit::Pose(Legs[k], LegPivots[k], FRotator(0.f, Swing, -Side * (Lift + Press)));
	}

	// Cola: se mece al sol, culebrea al correr y latiguea al contrario de la cabeza mordiendo.
	float TailYaw = 8.f * FMath::Sin(T * 1.2f) - 22.f * FMath::Min(1.f, Moving) * FMath::Sin(Phase - 0.8f);
	if (bShake)
	{
		TailYaw = -0.8f * HeadYaw + 6.f * FMath::Sin(T * 11.f);
	}
	TNBeachKit::Pose(Tail, TailPivot, FRotator(0.f, TailYaw, 0.f));

	// Lengua: sale a ratos; en el susto, fuera todo el rato; mordiendo, dentro; mareado, colgando.
	TongueTimer -= DeltaSeconds;
	TongueAge += DeltaSeconds;
	if (TongueTimer <= 0.f)
	{
		TongueTimer = 2.f + 2.5f * TNBeachKit::Hash01(static_cast<uint32>(T * 10.f) + static_cast<uint32>(Spec.Seed));
		TongueAge = 0.f;
	}
	float Out = TongueAge < 0.35f ? FMath::Sin(PI * TongueAge / 0.35f) : 0.f;
	if (bScare)
	{
		Out = 0.8f + 0.2f * FMath::Sin(T * 25.f);
	}
	if (bShake || bLunge)
	{
		Out = 0.f;
	}
	if (bDazed)
	{
		Out = 0.55f + 0.1f * FMath::Sin(T * 3.f);
	}
	if (Tongue)
	{
		TNBeachKit::Pose(Tongue, TonguePivot, FRotator(bDazed ? -40.f : -5.f, 6.f * FMath::Sin(T * 30.f), 0.f), FVector(FMath::Max(0.01f, Out), 1.f, 1.f));
		const bool bShowTongue = Out > 0.02f;
		if (Tongue->IsVisible() != bShowTongue)
		{
			Tongue->SetVisibility(bShowTongue);
		}
	}
}
