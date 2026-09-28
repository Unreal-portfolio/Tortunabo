#include "World/Beach/TN_BeachLizard.h"
#include "World/Beach/TN_BeachCameraShake.h"
#include "World/Beach/TN_BeachEnemySynth.h"
#include "TN_BeachEnemyKit.h"
#include "TN_BeachEnemyMeshes.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Player/TortugaCharacter.h"

namespace TNBeachLizardTuning
{
	/** Estados del lagarto (Mover.State). */
	enum class EState : uint8 { Bask, Alert, Scare, Flee, HideRock, Burrow, Hidden, Emerge, Walk };

	inline uint8 ToByte(EState E)
	{
		return static_cast<uint8>(E);
	}

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
}

ATN_BeachLizard::ATN_BeachLizard()
{
	NetFrequencyNear = 8.f;
	SetNetUpdateFrequency(NetFrequencyNear);
	bThrottleWhenFar = true;
}

float ATN_BeachLizard::GetBodyRadius() const
{
	return TNBeachLizardTuning::BodyRadius * SizeK;
}

void ATN_BeachLizard::ApplySpec()
{
	SizeK = FMath::Clamp(Spec.SizeScale, 0.8f, 1.15f);
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
	const TNFauna::FTNFaunaQuadLook Look = TNBeachMeshes::LizardLook(Pal);
	TArray<TNFauna::FTNFaunaPart> Parts;
	TNFauna::FTNFaunaRig FaunaRig;
	TNFauna::TNFaunaBuildQuad(Look, true, Parts, FaunaRig);
	RigHeight = static_cast<float>(FaunaRig.Height);
	TonguePivot = TNBeachMeshes::LizardTonguePivot(Look);
	for (FVector& P : LegPivots)
	{
		P = FVector::ZeroVector;
	}
	// Pivotes en todas las máquinas (la posición de la cabeza sirve al servidor para el empujón).
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
			const TNProcMesh::FTNProcMeshBuffers& Buffers = Part.Mesh;
			UStaticMesh* Mesh = TNBeachKit::CachedMesh(FString::Printf(TEXT("Beach.Lizard.%d.%d"), Pal, i), [&Buffers](TNProcMesh::FTNProcMeshBuffers& M) { M = Buffers; });
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
	for (TActorIterator<ATN_BeachElement> It(World); It; ++It)
	{
		const ATN_BeachElement* Element = *It;
		if (!Element || Element == this || !TNBeachLizardTuning::IsHideElement(Element->GetSpec().Element))
		{
			continue;
		}
		const FVector At = Element->GetActorLocation();
		FVector ToSpot = At - SimLoc;
		ToSpot.Z = 0.0;
		const double DistSq = ToSpot.SizeSquared();
		if (DistSq > BestSq)
		{
			continue;
		}
		// Nunca hacia la tortuga de la que huye.
		if (DistSq > 1.0 && FVector::DotProduct(ToSpot.GetSafeNormal(), ToThreat) > 0.35)
		{
			continue;
		}
		FVector Away = At - From;
		Away.Z = 0.0;
		OutSpot = At + Away.GetSafeNormal() * (Element->GetFootprintRadius() * 0.4f);
		BestSq = DistSq;
		bFound = true;
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

void ATN_BeachLizard::ServerTick(float DeltaSeconds)
{
	using TNBeachLizardTuning::EState;
	using TNBeachLizardTuning::ToByte;
	StateLeft -= DeltaSeconds;
	float Dist = 1.0e9f;
	ATortugaCharacter* Near = NearestTurtle(Dist);
	const float Alert = TNBeachLizardTuning::AlertRadius * SizeK;
	const float Scare = TNBeachLizardTuning::ScareRadius * SizeK;
	const float Panic = TNBeachLizardTuning::PanicRadius * SizeK;
	const EState State = static_cast<EState>(GetMoverState());
	const float Age = GetStateAge();

	switch (State)
	{
	case EState::Bask:
		if (Near && Dist < Alert)
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
				const float Reach = GetFootprintRadius() * TNBeachLizardTuning::RoamReach * FMath::Sqrt(ServerRng.FRandRange(0.25f, 1.f));
				const FVector Candidate = Home + FVector(FMath::Cos(Angle) * Reach, FMath::Sin(Angle) * Reach, 0.f);
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
		else if (Dist < Panic)
		{
			StartFlee(Near->GetActorLocation());
		}
		else if (Dist < Scare)
		{
			if (ServerRng.FRand() < TNBeachLizardTuning::ScareChance)
			{
				bShoved = false;
				ServerSetState(ToByte(EState::Scare), Near->GetActorLocation());
			}
			else
			{
				StartFlee(Near->GetActorLocation());
			}
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
		// Anda a su sitio o al siguiente rincón al sol (Mover.Aim); si una tortuga se acerca, se para a mirarla.
		if (Near && Dist < Scare)
		{
			StartFlee(Near->GetActorLocation());
		}
		else if (Near && Dist < Alert)
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
		DrawDebugCircle(World, SimLoc + FVector(0.0, 0.0, 30.0), Alert, 40, FColor::Yellow, false, -1.f, 0, 8.f, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), false);
		DrawDebugCircle(World, SimLoc + FVector(0.0, 0.0, 30.0), Scare, 40, FColor::Orange, false, -1.f, 0, 8.f, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), false);
		DrawDebugCircle(World, SimLoc + FVector(0.0, 0.0, 30.0), Panic, 32, FColor::Red, false, -1.f, 0, 8.f, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), false);
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
		Scaler->SetRelativeLocation(FVector(0.0, 0.0, -Sink * RigHeight * 1.3f * TNBeach::Scale * SizeK));
	}
	if (bNear && Sink < 0.98f)
	{
		PoseLizard(DeltaSeconds);
	}

	// Polvo al enterrarse y al salir.
	const bool bDigging = State == EState::Burrow || State == EState::Emerge || (State == EState::HideRock && Age < 0.6f);
	Dust.Origin = ShownLoc + FVector(0.0, 0.0, 60.0);
	Dust.Desc.Direction = FVector::UpVector;
	Dust.RateScale = bNear && bDigging ? 14.f : (bNear ? FMath::Clamp(Moving - 0.8f, 0.f, 1.f) * 6.f : 0.f);
	TNBeachKit::TickEmitterIfBusy(Dust, DeltaSeconds, View);

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
	const bool bAlert = State == EState::Alert || bScare;
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

	// Cuerpo: culebreo al correr, se hincha y tiembla en el susto, se sacude al enterrarse.
	const float Wiggle = 12.f * FMath::Min(1.f, Moving) * FMath::Sin(Phase);
	float Roll = 0.f;
	if (bScare)
	{
		Roll += 6.f * FMath::Sin(T * 40.f);
	}
	if (bDig)
	{
		Roll += 9.f * FMath::Sin(T * 32.f);
	}
	const float Puff = bScare ? 1.18f : 1.f + 0.02f * FMath::Sin(T * 1.6f);
	TNBeachKit::Pose(Body, BodyPivot + FVector(0.0, 0.0, PushUp), FRotator(bAlert ? 4.f : 0.f, Wiggle, Roll), FVector(1.f, Puff, Puff));

	// Cabeza: cabeceo tranquilo; alerta, mira a la tortuga con la cabeza alta.
	float HeadPitch = 5.f * FMath::Sin(T * 0.8f);
	float HeadYaw = -Wiggle * 0.6f;
	if (bAlert)
	{
		HeadPitch = bScare ? 24.f + 4.f * FMath::Sin(T * 30.f) : 14.f;
		const FVector ToAim = FVector(Mover.Aim) - ShownLoc;
		HeadYaw = FMath::Clamp(FMath::FindDeltaAngleDegrees(ShownYaw, static_cast<float>(ToAim.Rotation().Yaw)), -50.f, 50.f);
	}
	TNBeachKit::Pose(Head, HeadPivot, FRotator(HeadPitch, HeadYaw, 0.f));

	// Patas en diagonal (la delantera izquierda con la trasera derecha); en las flexiones, estiradas.
	for (int32 k = 0; k < Legs.Num(); ++k)
	{
		const bool bFront = k < 2;
		const float Side = (k % 2 == 0) ? -1.f : 1.f;
		const bool bPairA = (k == 0 || k == 3);
		const float LegPhase = Phase + (bPairA ? 0.f : PI);
		const float Swing = 28.f * FMath::Min(1.f, Moving) * FMath::Sin(LegPhase) * (bFront ? 1.f : -1.f);
		const float Lift = 18.f * FMath::Min(1.f, Moving) * FMath::Max(0.f, FMath::Cos(LegPhase)) + (bDig && bFront ? 25.f * FMath::Abs(FMath::Sin(T * 20.f)) : 0.f);
		const float Press = -PushUp * 14.f;
		TNBeachKit::Pose(Legs[k], LegPivots[k], FRotator(0.f, Swing, -Side * (Lift + Press)));
	}

	// Cola: se mece al sol y culebrea al correr.
	const float TailYaw = 8.f * FMath::Sin(T * 1.2f) - 22.f * FMath::Min(1.f, Moving) * FMath::Sin(Phase - 0.8f);
	TNBeachKit::Pose(Tail, TailPivot, FRotator(0.f, TailYaw, 0.f));

	// Lengua: sale a ratos; en el susto, fuera todo el rato.
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
	if (Tongue)
	{
		TNBeachKit::Pose(Tongue, TonguePivot, FRotator(-5.f, 6.f * FMath::Sin(T * 30.f), 0.f), FVector(FMath::Max(0.01f, Out), 1.f, 1.f));
		const bool bShowTongue = Out > 0.02f;
		if (Tongue->IsVisible() != bShowTongue)
		{
			Tongue->SetVisibility(bShowTongue);
		}
	}
}
