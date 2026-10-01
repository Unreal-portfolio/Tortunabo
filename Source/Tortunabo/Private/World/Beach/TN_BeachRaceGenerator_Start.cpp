// ─────────────────────────────────────────────────────────────────────────────
// ATN_BeachRaceGenerator — salida con huevos: dos filas de cuatro huevos en su nido de
// arena (las bases y el nido van con la salida, en TN_BeachRaceGenerator_Scenery.cpp),
// con la tortuga de cada jugador dentro durante la preparación y la cuenta atrás. Al dar
// la salida (OpenStartEggs, del GameMode), las tapas saltan dando vueltas hacia los lados,
// cada tortuga se ve 1 s en su huevo roto (se pone de pie, se sacude la cáscara y mira al
// mar: TNEggHatch, la pieza común con el cooperativo) y todas salen lanzadas a la vez hacia
// el mar, ya corriendo: como la salida de huevos del cooperativo (ATN_ProcStartStructure),
// pero en fila y con el salto hacia delante. En el sprint final de desempate el nido va a la
// línea del sprint (SetStartEggsAtSprint): sus bases y su anillo de arena se hacen allí
// (SprintNestMesh) y las tapas se mudan encima. Se replica solo si están rotos, desde cuándo
// y en qué línea (RoundNet); cada máquina anima sus tapas. Consola: TN.Beach.Egg repite la
// salida sin cambiar de ronda.
// ─────────────────────────────────────────────────────────────────────────────

#include "World/Beach/TN_BeachRaceGenerator.h"
#include "World/TN_EggHatch.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/TN_Log.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "ProceduralMeshComponent.h"
#include "TimerManager.h"
#include "TN_BeachRaceKit.h"

namespace TNBeachEggs
{
	/** Segundos entre que se rompe un huevo y el siguiente (de fuera adentro). */
	constexpr double HatchStagger = 0.08;
	/** Tapas: salto hacia arriba y hacia el lado (cm/s), gravedad (cm/s²), giros (grados/s) y lo que tardan en esfumarse. */
	constexpr double LidSpeedZ = 620.0;
	constexpr double LidSpeedSide = 360.0;
	constexpr double LidSpeedBack = 90.0;
	constexpr double LidGravity = 1400.0;
	constexpr double LidSpinYaw = 540.0;
	constexpr double LidTumble = 320.0;
	constexpr double LidShrinkSeconds = 0.3;
	/** Altura del origen de la tapa (la costura) sobre el suelo al posarse: los dientes bajan 18 cm. */
	constexpr double LidRestHeight = 20.0;
	/** Giro de cada tapa cerrada: múltiplo de 22,5° para que sus dientes encajen entre los de la base. */
	constexpr double LidClosedYawStep = 45.0;
	/** Lo que se hunde la base del huevo en la arena. */
	constexpr double CupSink = 8.0;
	/**
	 * Un cliente al que le llegan los huevos rotos con este retraso (s) sobre el lanzamiento (que va TNEggHatch::PauseSeconds
	 * después de romperse) todavía hace la pausa y salta; más tarde, los ve ya rotos.
	 */
	constexpr double LateLaunchGrace = 1.5;
	/** TN.Beach.Egg: segundos que se ven los huevos cerrados con las tortugas dentro antes de romperse otra vez. */
	constexpr float ReplayClosedSeconds = 1.5f;

	/** Origen de la tapa cerrada sobre la base Cup (local): el centro de su costura. */
	FVector LidClosed(const FVector& Cup)
	{
		return Cup + FVector(0.0, 0.0, TNCastleKit::EggSeam);
	}

	/** Hacia dónde salta la tapa de la base Cup: hacia fuera de la fila (los de -Y a -Y y los de +Y a +Y), un poco hacia atrás. */
	FVector LidDir(const FVector& Cup)
	{
		const double Side = Cup.Y >= 0.0 ? 1.0 : -1.0;
		return FVector(-LidSpeedBack, Side * LidSpeedSide, 0.0);
	}

	/** Segundos que vuela la tapa de la base Cup hasta posarse en la arena. */
	double LidFlightSeconds(const FVector& Cup)
	{
		const FVector Start = LidClosed(Cup);
		const FVector Land = Start + LidDir(Cup);
		const double Ground = TNBeachLayout::GroundZ(Land.X, Land.Y) + LidRestHeight;
		const double Drop = FMath::Max(0.0, Start.Z - Ground);
		return (LidSpeedZ + FMath::Sqrt(LidSpeedZ * LidSpeedZ + 2.0 * LidGravity * Drop)) / LidGravity;
	}

	/**
	 * Nido del huevo Index con el suelo en Spot (local): la base del huevo (la de los del lobby, medio enterrada) en Cups y
	 * el anillo de arena removida que se pisa en Ring. El mismo nido que el de la salida (TN_BeachRaceGenerator_Scenery.cpp).
	 */
	void AddNest(TNProcMesh::FTNProcMeshBuffers& Ring, TNProcMesh::FTNProcMeshBuffers& Cups, const FVector& Spot, int32 Index, TNArt::FPieceLog& Log)
	{
		{
			TNArt::FPieceScope CupPiece(Log, TN_ART("Beach.Start.EggCup"), TNArt::PiecePivot(Spot - FVector(0.0, 0.0, CupSink)), { &Cups });
			TNCastleKit::BuildEggCup(Cups, Spot - FVector(0.0, 0.0, CupSink), TNCastleKit::Col(0xFFF3DC), TNCastleKit::Col(TNCastleKit::EggAccent(Index)));
		}
		const FLinearColor Mound = TNBeachRaceKit::Hex(0xE2C58Eu);
		auto RingAt = [&Spot](double A, double R, double Z) { return FVector(Spot.X + FMath::Cos(A) * R, Spot.Y + FMath::Sin(A) * R, Spot.Z + Z); };
		constexpr int32 Seg = 20;
		TNArt::FPieceScope MoundPiece(Log, TN_ART("Beach.Start.NestMound"), TNArt::PiecePivot(Spot), { &Ring });
		for (int32 k = 0; k < Seg; ++k)
		{
			const double A0 = TNProcMap::TwoPi * k / Seg;
			const double A1 = TNProcMap::TwoPi * (k + 1) / Seg;
			const double Bump0 = 6.0 * TNProcMesh::TNProcHashNoise(k, Index, 0xE66u);
			const double Bump1 = 6.0 * TNProcMesh::TNProcHashNoise((k + 1) % Seg, Index, 0xE66u);
			const FVector Inward(-FMath::Cos(0.5 * (A0 + A1)), -FMath::Sin(0.5 * (A0 + A1)), 0.0);
			// Lomo del anillo (de 1,3 m y 30 cm de alto a 2,8 m, enterrado) y su cara de dentro, hacia el huevo.
			Ring.AddQuad(RingAt(A0, 130.0, 30.0 + Bump0), RingAt(A1, 130.0, 30.0 + Bump1), RingAt(A1, 280.0, -18.0), RingAt(A0, 280.0, -18.0), FVector::UpVector, Mound);
			Ring.AddQuad(RingAt(A0, 130.0, -20.0), RingAt(A1, 130.0, -20.0), RingAt(A1, 130.0, 30.0 + Bump1), RingAt(A0, 130.0, 30.0 + Bump0), Inward, Mound * 0.9f);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Línea de los huevos: la salida o la del sprint final
// ─────────────────────────────────────────────────────────────────────────────

FVector ATN_BeachRaceGenerator::StartEggCup(int32 Index) const
{
	if (!bEggsAtSprintLocal)
	{
		return TNBeachLayout::StartSpot(Index) - FVector(0.0, 0.0, TNBeachEggs::CupSink);
	}
	// En la línea del sprint, sobre el suelo con los asientos de la ronda (el mismo que GetSprintStartTransform).
	const FVector Spot = TNBeachLayout::SprintSpot(Index);
	const double Ground = TNBeachLayout::StampedZ(Layout.Stamps, Spot.X, Spot.Y, TNBeachLayout::SurfaceZ(Spot.X, Spot.Y));
	return FVector(Spot.X, Spot.Y, Ground - TNBeachEggs::CupSink);
}

bool ATN_BeachRaceGenerator::IsStartEggLineStale() const
{
	return RoundNet.bSprintEggs != bEggsAtSprintLocal || (bEggsAtSprintLocal && (SprintNestRound != AppliedRound || !IsValid(SprintNestMesh)));
}

void ATN_BeachRaceGenerator::ApplyStartEggLine()
{
	bEggsAtSprintLocal = RoundNet.bSprintEggs;
	SprintNestRound = AppliedRound;
	if (bEggsAtSprintLocal)
	{
		BuildSprintNest();
	}
	else if (IsValid(SprintNestMesh))
	{
		// De vuelta en la salida: sin nido (ni su colisión ni sus mallas de arte) en la línea del sprint.
		SprintNestMesh->ClearAllMeshSections();
		TNArt::ClearPieceArt(this, TEXT("SprintNest"));
	}
}

void ATN_BeachRaceGenerator::BuildSprintNest()
{
	if (!BeachRoot) { return; }
	if (!IsValid(SprintNestMesh))
	{
		UProceduralMeshComponent* Nest = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient | RF_DuplicateTransient);
		Nest->ComponentTags.Add(TNBeachRaceKit::GeneratedTag());
		// Colisión cocinada al momento: las finalistas aparecen dentro enseguida.
		Nest->bUseAsyncCooking = false;
		Nest->SetCanEverAffectNavigation(false);
		Nest->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Nest->SetupAttachment(BeachRoot);
		Nest->RegisterComponent();
		SprintNestMesh = Nest;
	}
	SprintNestMesh->ClearAllMeshSections();
	TNProcMesh::FTNProcMeshBuffers Ring;
	TNProcMesh::FTNProcMeshBuffers Cups;
	// Las mismas piezas de arte que el nido de la salida (Docs/Arte_Assets.md).
	TNArt::FPieceLog Log(TEXT("SprintNest"));
	for (int32 i = 0; i < TNBeachLayout::MaxStartEggs; ++i)
	{
		TNBeachEggs::AddNest(Ring, Cups, StartEggCup(i) + FVector(0.0, 0.0, TNBeachEggs::CupSink), i, Log);
	}
	// El anillo se pisa (con el material del terreno); las bases, con el de los huevos del lobby y sin colisión.
	TNBeachRaceKit::Upload(SprintNestMesh, 0, Ring, TNBeachRaceKit::TerrainMaterial(), true, &Log);
	TNBeachRaceKit::Upload(SprintNestMesh, 1, Cups, TNCastleKit::VertexColorMaterial(), false, &Log);
	TNArt::SpawnPieceArt(SprintNestMesh, Log);
}

void ATN_BeachRaceGenerator::SetStartEggsAtSprint(bool bAtSprint)
{
	if (!HasAuthority() || RoundNet.bSprintEggs == bAtSprint) { return; }
	RoundNet.bSprintEggs = bAtSprint;
	// En la línea nueva, cerrados: las tortugas aparecen dentro.
	RoundNet.bStartOpen = false;
	RoundNet.StartOpenTime = 0.f;
	ApplyStartEggs(false);
	ForceNetUpdate();
	UE_LOG(LogTortunabo, Log, TEXT("[Playa] ronda %d: los huevos de la salida van %s."), RoundNet.Round,
		bAtSprint ? *FString::Printf(TEXT("a la línea del sprint final (%.0f m)"), TNBeachLayout::SprintLineX() / 100.0) : TEXT("a la salida"));
}

// ─────────────────────────────────────────────────────────────────────────────
// Tapas
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachRaceGenerator::BuildStartEggs()
{
	if (!BeachRoot) { return; }
	// Tapas nuevas (y, si lo había, el nido del sprint lo ha quitado ClearGenerated): ApplyStartEggs las lleva a su línea.
	bEggsAtSprintLocal = false;
	SprintNestRound = -1;
	if (!IsValid(SprintNestMesh)) { SprintNestMesh = nullptr; }
	UMaterialInterface* Mat = TNCastleKit::VertexColorMaterial();
	for (int32 i = 0; i < TNBeachLayout::MaxStartEggs; ++i)
	{
		// La misma tapa que los huevos del lobby y de la salida del cooperativo, con el color de cada huevo.
		TNProcMesh::FTNProcMeshBuffers LidBuffers;
		TNCastleKit::BuildEggLid(LidBuffers, TNCastleKit::Pal(0xFFF3DC), TNCastleKit::Pal(TNCastleKit::EggAccent(i)));
		UStaticMesh* LidMesh = TNProcRuntimeMesh::MakeStaticMesh(this, LidBuffers, Mat);
		if (!LidMesh) { continue; }
		StartEggLidMeshes.Add(LidMesh);
		UStaticMeshComponent* Lid = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient | RF_DuplicateTransient);
		Lid->ComponentTags.Add(TNBeachRaceKit::GeneratedTag());
		Lid->SetStaticMesh(LidMesh);
		Lid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Lid->SetCanEverAffectNavigation(false);
		Lid->SetMobility(EComponentMobility::Movable);
		Lid->SetupAttachment(BeachRoot);
		Lid->RegisterComponent();
		// Ya registrada: la malla de arte de la tapa (si la hay) va de hija y salta y se esfuma con ella.
		TNArt::ApplyToComponent(Lid, TN_ART("Beach.Start.EggLid"));
		Lid->SetRelativeLocationAndRotation(TNBeachEggs::LidClosed(StartEggCup(i)), FRotator(0.0, TNBeachEggs::LidClosedYawStep * i, 0.0));
		StartEggLids.Add(Lid);
	}
}

void ATN_BeachRaceGenerator::OpenStartEggs()
{
	if (!HasAuthority() || RoundNet.bStartOpen) { return; }
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	RoundNet.bStartOpen = true;
	const double ServerNow = GS ? GS->GetServerWorldTimeSeconds() : (World ? static_cast<double>(World->GetTimeSeconds()) : 0.0);
	RoundNet.StartOpenTime = static_cast<float>(ServerNow);
	ForceNetUpdate();
	ApplyStartEggs(true);
	UE_LOG(LogTortunabo, Log, TEXT("[Playa] ronda %d: se rompen los huevos %s."), RoundNet.Round, RoundNet.bSprintEggs ? TEXT("del sprint final") : TEXT("de la salida"));
}

void ATN_BeachRaceGenerator::ApplyStartEggs(bool bLive)
{
	// Primero, en su línea (la salida o la del sprint final).
	if (IsStartEggLineStale()) { ApplyStartEggLine(); }
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	bEggsOpenLocal = RoundNet.bStartOpen;
	EggsHatchedMask = 0;
	// Un cliente los vive (pausa en el huevo y salto) si le llegan antes de que pase el lanzamiento, que va
	// TNEggHatch::PauseSeconds después de romperse, con algo de margen; si no, los ve ya rotos.
	bool bLiveNow = bLive;
	if (!bLiveNow && bEggsOpenLocal && !HasAuthority() && World && World->GetGameState())
	{
		const double Since = TNEggHatch::ServerNow(World) - static_cast<double>(RoundNet.StartOpenTime);
		bLiveNow = Since < TNEggHatch::PauseSeconds + TNBeachEggs::LateLaunchGrace;
	}
	bEggsLaunch = bLiveNow && bEggsOpenLocal;
	if (!bEggsOpenLocal)
	{
		// Cerrados: cada tapa sobre su base.
		bEggsAnimating = false;
		for (int32 i = 0; i < StartEggLids.Num(); ++i)
		{
			UStaticMeshComponent* Lid = StartEggLids[i];
			if (!Lid) { continue; }
			Lid->SetRelativeLocationAndRotation(TNBeachEggs::LidClosed(StartEggCup(i)), FRotator(0.0, TNBeachEggs::LidClosedYawStep * i, 0.0));
			Lid->SetRelativeScale3D(FVector::OneVector);
			Lid->SetVisibility(true);
		}
		// Y ninguna tortuga a medias de su pausa en esta máquina (TN.Beach.Egg o una ronda nueva en plena pausa): la pose,
		// como estaba, y sin soltarla (quien la coloca ya la sujeta).
		if (World && World->IsGameWorld())
		{
			for (TActorIterator<ACharacter> It(World); It; ++It)
			{
				if (TNEggHatch::IsHatching(*It)) { TNEggHatch::Cancel(*It, false); }
			}
		}
		return;
	}
	// Rotos: al vivirlo, desde ahora (con la pausa en el huevo y el salto de las tortugas); si no, ya del todo.
	EggsOpenedAt = bLiveNow ? Now : Now - 1000.0;
	if (bEggsLaunch) { LaunchTurtlesFromEggs(); }
	bEggsAnimating = UpdateStartEggs();
}

bool ATN_BeachRaceGenerator::UpdateStartEggs()
{
	using namespace TNBeachEggs;
	const UWorld* World = GetWorld();
	const double Elapsed = (World ? World->GetTimeSeconds() : EggsOpenedAt) - EggsOpenedAt;
	bool bRunning = false;
	const int32 Num = StartEggLids.Num();
	for (int32 i = 0; i < Num; ++i)
	{
		UStaticMeshComponent* Lid = StartEggLids[i];
		if (!Lid) { continue; }
		// De fuera adentro: primero los de las puntas de cada fila; la de detrás, un paso después que la de delante.
		const int32 EggColumn = i % TNBeachLayout::NumStartSpots;
		const int32 Order = FMath::Min(EggColumn, TNBeachLayout::NumStartSpots - 1 - EggColumn) + i / TNBeachLayout::NumStartSpots;
		const double T = Elapsed - HatchStagger * Order;
		if (T < 0.0)
		{
			bRunning = true;
			continue;
		}
		EggsHatchedMask |= 1 << i;
		const FVector Cup = StartEggCup(i);
		const double Flight = LidFlightSeconds(Cup);
		const double Shrink = T > Flight ? 1.0 - (T - Flight) / LidShrinkSeconds : 1.0;
		if (Shrink <= 0.0)
		{
			Lid->SetVisibility(false);
			continue;
		}
		// Tiro parabólico hacia fuera dando vueltas; al posarse se queda donde cae y encoge hasta desaparecer.
		const double Tf = FMath::Min(T, Flight);
		const FVector Where = LidClosed(Cup) + LidDir(Cup) * Tf + FVector(0.0, 0.0, LidSpeedZ * Tf - 0.5 * LidGravity * Tf * Tf);
		Lid->SetVisibility(true);
		Lid->SetRelativeLocationAndRotation(Where, FRotator(LidTumble * Tf, LidClosedYawStep * i + LidSpinYaw * Tf, 0.0));
		Lid->SetRelativeScale3D(FVector(Shrink));
		bRunning = true;
	}
	return bRunning;
}

void ATN_BeachRaceGenerator::LaunchTurtlesFromEggs()
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld()) { return; }
	const FTransform& Xf = GetActorTransform();
	const FVector Launch = Xf.TransformVectorNoScale(FVector(TNBeachLayout::EggLaunchForward, 0.0, 0.0)) + FVector(0.0, 0.0, TNBeachLayout::EggLaunchUp);
	// La franja de la línea de los huevos (la salida o la del sprint), con las filas de detrás: la de la salida, desde el
	// muro de detrás hasta EggLaunchReachX; la del sprint, la misma franja movida a su línea.
	const double LineShift = bEggsAtSprintLocal ? TNBeachLayout::SprintLineX() - TNBeachLayout::StartSpotX : 0.0;
	const double MinX = TNBeachLayout::BackWallX + LineShift;
	const double MaxX = TNBeachLayout::EggLaunchReachX + LineShift;
	// Con el reloj del servidor: la pausa empieza al romperse y el lanzamiento, TNEggHatch::PauseSeconds después, a la vez
	// para todas (es una carrera: nadie sale antes por su huevo).
	const double HatchAt = static_cast<double>(RoundNet.StartOpenTime);
	const double LaunchAt = HatchAt + TNEggHatch::PauseSeconds;
	const float SeaYaw = static_cast<float>(GetActorRotation().Yaw);
	// Cada tortuga de la franja, en cada máquina: la pausa (ponerse de pie y sacudirse) se ve en todas; la sujetan y la
	// lanzan a la vez solo quienes la mueven, el servidor (a todas) y cada cliente (a la suya), como antes.
	int32 Hatched = 0;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		ACharacter* Turtle = *It;
		if (!IsValid(Turtle) || !Turtle->IsPlayerControlled()) { continue; }
		const FVector Local = Xf.InverseTransformPosition(Turtle->GetActorLocation());
		if (Local.X < MinX || Local.X > MaxX || FMath::Abs(Local.Y) > TNBeachLayout::HalfWidth) { continue; }
		// Los trocitos de cáscara, del color del huevo más cercano.
		int32 Egg = 0;
		double BestDistSq = TNumericLimits<double>::Max();
		for (int32 i = 0; i < TNBeachLayout::MaxStartEggs; ++i)
		{
			const double DistSq = FVector::DistSquared2D(Local, StartEggCup(i));
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Egg = i;
			}
		}
		TNEggHatch::Begin(Turtle, HatchAt, LaunchAt, Launch, SeaYaw, TNCastleKit::EggAccent(Egg));
		++Hatched;
	}
	UE_LOG(LogTortunabo, Verbose, TEXT("[Playa] huevos: %d tortugas en la pausa del huevo antes de salir lanzadas hacia el mar."), Hatched);
}

// ─────────────────────────────────────────────────────────────────────────────
// TN.Beach.Egg: repetir la salida sin cambiar de ronda
// ─────────────────────────────────────────────────────────────────────────────

void ATN_BeachRaceGenerator::ReplayStartEggs()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !World->IsGameWorld() || StartEggLids.Num() == 0) { return; }
	GetWorldTimerManager().ClearTimer(EggReplayHandle);
	// Cerrados otra vez en la línea en la que estén (la salida o la del sprint final).
	RoundNet.bStartOpen = false;
	RoundNet.StartOpenTime = 0.f;
	ApplyStartEggs(false);
	ForceNetUpdate();

	// Cada tortuga dentro de un huevo (por orden de jugador), quieta y mirando al mar, como al preparar la ronda.
	FCollisionObjectQueryParams Floors;
	Floors.AddObjectTypesToQuery(ECC_WorldStatic);
	Floors.AddObjectTypesToQuery(ECC_WorldDynamic);
	int32 Slot = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		ACharacter* Turtle = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
		if (!Turtle) { continue; }
		// Sin la pausa de antes, si aún estaba en ella.
		TNEggHatch::Cancel(Turtle);
		const FTransform Spot = bEggsAtSprintLocal ? GetSprintStartTransform(Slot) : GetStartTransform(Slot);
		++Slot;
		// De pie en el suelo del huevo, con el alto de su cápsula (como PutOnFloor del GameMode).
		FVector Where = Spot.GetLocation();
		const UCapsuleComponent* Capsule = Turtle->GetCapsuleComponent();
		const double Half = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.0;
		FHitResult Hit;
		const FCollisionQueryParams Query(SCENE_QUERY_STAT(TNBeachEggReplay), false, Turtle);
		if (World->LineTraceSingleByObjectType(Hit, Where + FVector(0.0, 0.0, 300.0), Where - FVector(0.0, 0.0, 1500.0), Floors, Query)
			&& Hit.ImpactNormal.Z > 0.5)
		{
			Where.Z = Hit.ImpactPoint.Z + Half + 2.0;
		}
		UCharacterMovementComponent* Move = Turtle->GetCharacterMovement();
		if (Move) { Move->StopMovementImmediately(); }
		Turtle->SetActorLocationAndRotation(Where, Spot.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
		if (Move) { Move->DisableMovement(); }
		PC->ClientSetRotation(Spot.Rotator(), true);
	}
	UE_LOG(LogTortunabo, Log, TEXT("[Playa] TN.Beach.Egg: %d tortugas otra vez en los huevos %s; se rompen en %.1f s."), Slot,
		bEggsAtSprintLocal ? TEXT("del sprint final") : TEXT("de la salida"), TNBeachEggs::ReplayClosedSeconds);
	// Un momento con los huevos cerrados y la salida de siempre: se rompen, 1 s en el huevo y todas lanzadas.
	GetWorldTimerManager().SetTimer(EggReplayHandle, this, &ATN_BeachRaceGenerator::OpenStartEggs, TNBeachEggs::ReplayClosedSeconds, false);
}

namespace TNBeachEggs
{
	/** El mundo con autoridad del mismo proceso (el propio si no es un cliente; en PIE, el del servidor del mismo mapa). */
	UWorld* FindAuthorityWorld(UWorld* InWorld)
	{
		if (!InWorld)
		{
			return nullptr;
		}
		if (InWorld->GetNetMode() != NM_Client)
		{
			return InWorld;
		}
		if (!GEngine)
		{
			return nullptr;
		}
		const FString MapName = UWorld::RemovePIEPrefix(InWorld->GetMapName());
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* Candidate = Context.World();
			if (Candidate && Candidate != InWorld && Candidate->IsGameWorld() && Candidate->GetNetMode() != NM_Client
				&& UWorld::RemovePIEPrefix(Candidate->GetMapName()) == MapName)
			{
				return Candidate;
			}
		}
		return nullptr;
	}

	void RunReplay(const TArray<FString>& /*Args*/, UWorld* InWorld)
	{
		UWorld* AuthWorld = FindAuthorityWorld(InWorld);
		ATN_BeachRaceGenerator* Generator = AuthWorld ? ATN_BeachRaceGenerator::Find(AuthWorld) : nullptr;
		if (!Generator)
		{
			UE_LOG(LogTortunabo, Warning, TEXT("[Playa] TN.Beach.Egg: sin playa de la carrera con autoridad (escríbelo en la ventana del anfitrión, en LVL_BeachRace)."));
			return;
		}
		Generator->ReplayStartEggs();
	}

	static FAutoConsoleCommandWithWorldAndArgs ReplayCommand(
		TEXT("TN.Beach.Egg"),
		TEXT("Carrera: cierra otra vez los huevos con las tortugas dentro y repite la salida (se rompen, 1 s en el huevo y salen lanzadas), sin cambiar de ronda. En el anfitrión."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunReplay),
		ECVF_Cheat);
}
