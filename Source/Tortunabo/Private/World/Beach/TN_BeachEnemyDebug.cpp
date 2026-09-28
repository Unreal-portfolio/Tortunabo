// ─────────────────────────────────────────────────────────────────────────────
// Consola de pruebas de los enemigos y la tormenta de la playa. Desde la ventana de un cliente del PIE van al mundo del
// servidor del mismo proceso; en un cliente remoto de verdad no hacen nada (hay que escribirlos en el anfitrión).
//   TN.Beach.Quad.Now                    todos los pasos de quads empiezan su aviso ya.
//   TN.Beach.Gull.Attack [1|2]           cada zona de gaviotas ataca ya a la tortuga más cercana (1 cagada, 2 picado).
//   TN.Beach.Storm.Start [Metros] [Speed] arranca la tormenta (la crea si no hay, detrás de ti mirando hacia donde miras)
//                                        con el frente Metros por detrás de ti (30 por defecto) a Speed cm/s (180).
//   TN.Beach.Storm.Stop                  la para (se queda a la vista).
//   TN.Beach.Storm.Info                  frente, velocidad, a qué velocidad va y distancia a la última tortuga.
//   TN.Beach.Storm.Here [jugador] [m]    pone el frente 4 m (o m) por delante de tu tortuga o de la del jugador N (índice en
//                                        PlayerArray): la deja dentro y le llega la patada.
//   TN.Beach.Lizard <huidizo|generoso|mordedor>  un lagarto de ese carácter 22 m delante de ti.
//   TN.Beach.StunNearest [s]             marea al enemigo más cercano a tu tortuga (3 s por defecto; los quads no).
//   TN.Beach.Enemy.Stats                 enemigos del mundo, cuántos van despacio por estar lejos y cuántos se apartan.
//   TN.Beach.Enemy.Debug 1               (CVar) radios, oído, recorridos y estados en el servidor.
// Para crear enemigos sueltos: TN.Beach.Place GiantCrab (SeaUrchin, Lizard, QuadLane, GullZone), del generador.
// ─────────────────────────────────────────────────────────────────────────────

#include "World/Beach/TN_BeachGullZone.h"
#include "World/Beach/TN_BeachLizard.h"
#include "World/Beach/TN_BeachQuadLane.h"
#include "World/Beach/TN_BeachStorm.h"
#include "CollisionQueryParams.h"
#include "Core/TN_Log.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"

namespace TNBeachEnemyConsole
{
	/** El mundo con autoridad del mismo proceso (el propio si no es un cliente; en el PIE, el del servidor del mismo mapa). */
	UWorld* AuthorityWorld(UWorld* InWorld)
	{
		if (!InWorld)
		{
			return nullptr;
		}
		if (InWorld->GetNetMode() != NM_Client)
		{
			return InWorld;
		}
		if (GEngine)
		{
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
		}
		UE_LOG(LogTortunabo, Warning, TEXT("[Playa] Los comandos de los enemigos van en la ventana del anfitrión."));
		return nullptr;
	}

	/** Tortuga del primer jugador local (de la ventana donde se escribe). */
	APawn* LocalPawn(const UWorld* World)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? PC->GetPawn() : nullptr;
	}

	void QuadNow(const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = AuthorityWorld(InWorld);
		if (!World)
		{
			return;
		}
		int32 Count = 0;
		for (TActorIterator<ATN_BeachQuadLane> It(World); It; ++It)
		{
			It->DebugPassNow();
			++Count;
		}
		UE_LOG(LogTortunabo, Log, TEXT("[Playa] TN.Beach.Quad.Now: %d pasos de quads."), Count);
	}

	void GullAttack(const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = AuthorityWorld(InWorld);
		if (!World)
		{
			return;
		}
		const int32 Kind = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
		int32 Count = 0;
		for (TActorIterator<ATN_BeachGullZone> It(World); It; ++It)
		{
			It->DebugAttackNow(Kind);
			++Count;
		}
		UE_LOG(LogTortunabo, Log, TEXT("[Playa] TN.Beach.Gull.Attack: %d zonas de gaviotas."), Count);
	}

	void StormStart(const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = AuthorityWorld(InWorld);
		if (!World)
		{
			return;
		}
		const APawn* Pawn = LocalPawn(InWorld);
		const float Behind = (Args.Num() > 0 ? FCString::Atof(*Args[0]) : 30.f) * 100.f;
		const float Speed = Args.Num() > 1 ? FCString::Atof(*Args[1]) : GetDefault<ATN_BeachStorm>()->DefaultSpeed;
		ATN_BeachStorm* Storm = ATN_BeachStorm::FindStorm(World);
		if (!Storm)
		{
			const FVector Here = Pawn ? Pawn->GetActorLocation() - FVector(0.0, 0.0, 90.0) : FVector::ZeroVector;
			const FRotator Facing(0.f, Pawn ? Pawn->GetActorRotation().Yaw : 0.f, 0.f);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Storm = World->SpawnActor<ATN_BeachStorm>(ATN_BeachStorm::StaticClass(), Here, Facing, Params);
			if (Storm)
			{
				Storm->StartStormAt(-Behind, Speed, 0.f);
			}
			return;
		}
		// Ya hay una: el frente sale Metros por detrás de la tortuga a lo largo del eje de la tormenta.
		float Offset = -Behind;
		if (Pawn)
		{
			const FVector Local = Storm->GetActorTransform().InverseTransformPositionNoScale(Pawn->GetActorLocation());
			Offset = static_cast<float>(Local.X) - Behind;
		}
		Storm->StartStormAt(Offset, Speed, 0.f);
	}

	void StormStop(const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = AuthorityWorld(InWorld);
		if (!World)
		{
			return;
		}
		if (ATN_BeachStorm* Storm = ATN_BeachStorm::FindStorm(World))
		{
			Storm->StopStorm();
		}
	}

	void StormInfo(const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = AuthorityWorld(InWorld);
		const ATN_BeachStorm* Storm = World ? ATN_BeachStorm::FindStorm(World) : nullptr;
		UE_LOG(LogTortunabo, Log, TEXT("[Playa] TN.Beach.Storm.Info: %s"), Storm ? *Storm->DescribeState() : TEXT("no hay tormenta"));
	}

	void StormHere(const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = AuthorityWorld(InWorld);
		const APawn* Pawn = LocalPawn(InWorld);
		// Con un número, la tortuga de ese jugador (índice en PlayerArray del anfitrión): así se prueba también la del cliente.
		if (World && Args.Num() > 0)
		{
			const AGameStateBase* GameState = World->GetGameState();
			const int32 Index = FCString::Atoi(*Args[0]);
			const APlayerState* PlayerState = GameState && GameState->PlayerArray.IsValidIndex(Index) ? GameState->PlayerArray[Index].Get() : nullptr;
			Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
		}
		// Cuánto por detrás del frente la deja (m; 4 por defecto; más, para probar patadas largas o el salto).
		const float Inside = (Args.Num() > 1 ? FCString::Atof(*Args[1]) : 4.f) * 100.f;
		if (!World || !Pawn)
		{
			UE_LOG(LogTortunabo, Warning, TEXT("[Playa] TN.Beach.Storm.Here: no hay esa tortuga."));
			return;
		}
		ATN_BeachStorm* Storm = ATN_BeachStorm::FindStorm(World);
		if (!Storm)
		{
			// Sin tormenta (otro mapa): una detrás de ti, mirando hacia donde miras.
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FRotator Facing(0.f, Pawn->GetActorRotation().Yaw, 0.f);
			Storm = World->SpawnActor<ATN_BeachStorm>(ATN_BeachStorm::StaticClass(), Pawn->GetActorLocation() - Facing.Vector() * 3000.0 - FVector(0.0, 0.0, 90.0), Facing, Params);
		}
		if (!Storm)
		{
			return;
		}
		// El frente Inside por delante de la tortuga: se queda dentro y le llega la patada.
		const FVector Local = Storm->GetActorTransform().InverseTransformPositionNoScale(Pawn->GetActorLocation());
		Storm->DebugSetFront(static_cast<float>(Local.X) + Inside);
	}

	void LizardHere(const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = AuthorityWorld(InWorld);
		const APawn* Pawn = LocalPawn(InWorld);
		if (!World || !Pawn)
		{
			return;
		}
		ETNBeachLizardTemper Temper = ETNBeachLizardTemper::Shy;
		const FString Kind = Args.Num() > 0 ? Args[0].ToLower() : FString(TEXT("huidizo"));
		if (Kind.StartsWith(TEXT("gen")))
		{
			Temper = ETNBeachLizardTemper::Generous;
		}
		else if (Kind.StartsWith(TEXT("mor")) || Kind.StartsWith(TEXT("bit")))
		{
			Temper = ETNBeachLizardTemper::Biter;
		}
		// 22 m delante (lo bastante lejos para que no huya ni muerda al aparecer), apoyado en el suelo.
		const FRotator Facing(0.f, Pawn->GetActorRotation().Yaw, 0.f);
		FVector At = Pawn->GetActorLocation() + Facing.Vector() * 2200.0;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(TNBeachLizardHere), false);
		if (World->LineTraceSingleByObjectType(Hit, At + FVector(0.0, 0.0, 3000.0), At - FVector(0.0, 0.0, 6000.0), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			At.Z = Hit.ImpactPoint.Z;
		}
		else
		{
			At.Z -= 90.0;
		}
		FTNBeachElementSpec LizardSpec;
		LizardSpec.Element = ETNBeachElement::Lizard;
		LizardSpec.Seed = ATN_BeachLizard::FindSeedForTemper(Temper, FMath::Rand());
		LizardSpec.SizeScale = 1.f;
		// Mirando hacia la tortuga.
		ATN_BeachElement* Spawned = ATN_BeachElement::SpawnElement(World, FTransform(FRotator(0.f, Facing.Yaw + 180.f, 0.f), At), LizardSpec);
		if (Spawned)
		{
			Spawned->Tags.AddUnique(FName(TEXT("TNBeachDebug")));
		}
		UE_LOG(LogTortunabo, Log, TEXT("[Playa] TN.Beach.Lizard: lagarto %s (semilla %d) %s."), ATN_BeachLizard::TemperName(Temper), LizardSpec.Seed,
			Spawned ? TEXT("delante") : TEXT("no se ha podido crear"));
	}

	void StunNearest(const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = AuthorityWorld(InWorld);
		const APawn* Pawn = LocalPawn(InWorld);
		if (!World)
		{
			return;
		}
		const float Seconds = Args.Num() > 0 ? FMath::Max(0.1f, FCString::Atof(*Args[0])) : TNBeachHitStun::ThrownSeconds;
		const FVector From = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
		ATN_BeachEnemy* Best = nullptr;
		double BestSq = 1.0e20;
		for (TActorIterator<ATN_BeachEnemy> It(World); It; ++It)
		{
			ATN_BeachEnemy* Enemy = *It;
			if (!Enemy || !Enemy->AcceptsHitStun())
			{
				continue;
			}
			// Donde está su cuerpo (los que andan no mueven el actor); sin cuerpo al que dar, el actor.
			FVector A;
			FVector B;
			float Radius = 0.f;
			const FVector At = Enemy->GetHitCapsule(A, B, Radius) ? (A + B) * 0.5 : Enemy->GetActorLocation();
			const double DistSq = FVector::DistSquared(At, From);
			if (DistSq < BestSq)
			{
				BestSq = DistSq;
				Best = Enemy;
			}
		}
		if (Best)
		{
			Best->ApplyHitStun(Seconds, nullptr);
		}
		UE_LOG(LogTortunabo, Log, TEXT("[Playa] TN.Beach.StunNearest: %s mareado %.1f s (a %.0f m)."), Best ? *Best->GetName() : TEXT("ningún enemigo"),
			Seconds, Best ? FMath::Sqrt(BestSq) / 100.0 : 0.0);
	}

	void EnemyStats(const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = AuthorityWorld(InWorld);
		if (!World)
		{
			return;
		}
		int32 Total = 0;
		int32 Throttled = 0;
		int32 Movers = 0;
		ATN_BeachEnemy::GatherStats(World, Total, Throttled, Movers);
		UE_LOG(LogTortunabo, Log, TEXT("[Playa] TN.Beach.Enemy.Stats: %d enemigos, %d despacio por estar lejos, %d que andan y se apartan."),
			Total, Throttled, Movers);
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdBeachQuadNow(TEXT("TN.Beach.Quad.Now"),
		TEXT("Todos los pasos de quads empiezan su aviso ya (en el anfitrión)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&QuadNow), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs CmdBeachGullAttack(TEXT("TN.Beach.Gull.Attack"),
		TEXT("Cada zona de gaviotas ataca ya a la tortuga más cercana: 1 cagada, 2 picado, nada = al azar (en el anfitrión)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&GullAttack), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs CmdBeachStormStart(TEXT("TN.Beach.Storm.Start"),
		TEXT("Arranca la tormenta de bañistas: TN.Beach.Storm.Start [metros por detrás=30] [cm/s=180] (en el anfitrión)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StormStart), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs CmdBeachStormStop(TEXT("TN.Beach.Storm.Stop"),
		TEXT("Para la tormenta de bañistas (en el anfitrión)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StormStop), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs CmdBeachStormInfo(TEXT("TN.Beach.Storm.Info"),
		TEXT("Frente, velocidad y distancia a la última tortuga de la tormenta de bañistas (en el anfitrión)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StormInfo), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs CmdBeachStormHere(TEXT("TN.Beach.Storm.Here"),
		TEXT("Pone el frente de la tormenta por delante de una tortuga para ver la patada: TN.Beach.Storm.Here [jugador = la tuya] [metros = 4] (en el anfitrión)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StormHere), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs CmdBeachLizard(TEXT("TN.Beach.Lizard"),
		TEXT("Un lagarto delante de tu tortuga: TN.Beach.Lizard <huidizo|generoso|mordedor> (en el anfitrión; TN.Beach.Place clear lo quita)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&LizardHere), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs CmdBeachStunNearest(TEXT("TN.Beach.StunNearest"),
		TEXT("Marea al enemigo de la playa más cercano a tu tortuga: TN.Beach.StunNearest [segundos=3] (en el anfitrión; los quads no)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StunNearest), ECVF_Cheat);

	static FAutoConsoleCommandWithWorldAndArgs CmdBeachEnemyStats(TEXT("TN.Beach.Enemy.Stats"),
		TEXT("Cuántos enemigos de la playa hay, cuántos van despacio por estar lejos y cuántos se apartan (en el anfitrión)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&EnemyStats), ECVF_Cheat);
}
