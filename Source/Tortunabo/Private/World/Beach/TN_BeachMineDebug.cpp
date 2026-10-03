#include "World/Beach/TN_BeachMine.h"
#include "World/Beach/TN_BeachElement.h"
#include "World/Beach/TN_BeachTypes.h"
#include "Core/TN_Log.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "TN_BeachTrapKit.h"

/**
 * Prueba de red de la mina (#18), en el servidor:
 *
 *   TN.Beach.Mine.Blast <jugador> [metros=0] [veces=1] [cada=6] [espera=cada]
 *
 * Pone una mina a «metros» detrás de la tortuga del jugador (índice en PlayerArray del anfitrión: 1 = el primer cliente)
 * y la hace saltar como si la pisaran; repite «veces» veces, cada «cada» segundos. Con 0 m la lanza en bola; con 2-6 m la
 * empuja. Espera a que el jugador tenga tortuga libre (fuera de la bola, del caparazón y del aturdimiento). Para contar
 * correcciones: p.NetShowCorrections 1 en el servidor y en el cliente («*** Server: Error» / «*** Client: Error» en el
 * registro) y NetEmulation.PktLag 120 en el cliente. Las minas así creadas las borra TN.Beach.Place clear.
 */
namespace TNBeachMineDebug
{
	const FName DebugTag(TEXT("TNBeachDebug"));

	struct FBlastRun
	{
		TWeakObjectPtr<UWorld> World;
		int32 PlayerIndex = 1;
		double Meters = 0.0;
		int32 Remaining = 0;
		FTimerHandle Timer;
	};

	FBlastRun Run;

	ACharacter* FindPlayerCharacter(UWorld* World, int32 PlayerIndex)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		const APlayerState* PlayerState = GameState && GameState->PlayerArray.IsValidIndex(PlayerIndex) ? GameState->PlayerArray[PlayerIndex].Get() : nullptr;
		return PlayerState ? Cast<ACharacter>(PlayerState->GetPawn()) : nullptr;
	}

	/** Detrás de la tortuga, apoyado en el suelo (sin personajes). */
	FTransform MineTransform(UWorld* World, const ACharacter* Turtle, double Meters)
	{
		const FRotator Facing(0.0, Turtle->GetActorRotation().Yaw, 0.0);
		FVector At = Turtle->GetActorLocation() - Facing.Vector() * (Meters * 100.0);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(TNBeachMineDebug), false);
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		if (World->LineTraceSingleByObjectType(Hit, At + FVector(0.0, 0.0, 300.0), At - FVector(0.0, 0.0, 600.0), Objects, Params))
		{
			At.Z = Hit.ImpactPoint.Z;
		}
		return FTransform(Facing, At);
	}

	void BlastOnce()
	{
		UWorld* World = Run.World.Get();
		ACharacter* Turtle = FindPlayerCharacter(World, Run.PlayerIndex);
		if (!World || Run.Remaining <= 0)
		{
			if (World)
			{
				World->GetTimerManager().ClearTimer(Run.Timer);
			}
			return;
		}
		if (!TNBeachTrapKit::IsFreeTurtle(Turtle))
		{
			UE_LOG(LogTortunabo, Log, TEXT("[Playa] TN.Beach.Mine.Blast: el jugador %d aún no tiene tortuga libre; se espera."), Run.PlayerIndex);
			return;
		}
		FTNBeachElementSpec Spec;
		Spec.Element = ETNBeachElement::Mine;
		Spec.Seed = Run.Remaining;
		Spec.SizeScale = 1.f;
		ATN_BeachMine* Mine = Cast<ATN_BeachMine>(ATN_BeachElement::SpawnElement(World, MineTransform(World, Turtle, Run.Meters), Spec));
		if (!Mine || !Mine->TriggerForTest())
		{
			UE_LOG(LogTortunabo, Warning, TEXT("[Playa] TN.Beach.Mine.Blast: no se ha podido crear o disparar la mina."));
			return;
		}
		Mine->Tags.AddUnique(DebugTag);
		--Run.Remaining;
		UE_LOG(LogTortunabo, Log, TEXT("[Playa] TN.Beach.Mine.Blast: %s a %.1f m de %s (quedan %d)."), *Mine->GetName(), Run.Meters, *Turtle->GetName(),
			Run.Remaining);
	}

	void RunBlast(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogTortunabo, Warning, TEXT("[Playa] TN.Beach.Mine.Blast: solo en el servidor."));
			return;
		}
		if (const UWorld* Previous = Run.World.Get())
		{
			Previous->GetTimerManager().ClearTimer(Run.Timer);
		}
		Run.World = World;
		Run.PlayerIndex = Args.Num() > 0 ? FMath::Max(0, FCString::Atoi(*Args[0])) : 1;
		Run.Meters = Args.Num() > 1 ? FMath::Max(0.0, FCString::Atod(*Args[1])) : 0.0;
		Run.Remaining = Args.Num() > 2 ? FMath::Max(1, FCString::Atoi(*Args[2])) : 1;
		const float Interval = Args.Num() > 3 ? FMath::Max(1.f, FCString::Atof(*Args[3])) : 6.f;
		// La primera, tras «espera» segundos: con -TNMineBlast el cliente aún está montando la ronda al empezar.
		const float FirstDelay = Args.Num() > 4 ? FMath::Max(0.1f, FCString::Atof(*Args[4])) : Interval;
		World->GetTimerManager().SetTimer(Run.Timer, FTimerDelegate::CreateStatic(&BlastOnce), Interval, true, FirstDelay);
	}

	/** Con -game no se ejecuta -ExecCmds: -TNMineBlast=<jugador>_<metros>_<veces>_<cada>_<espera> lo lanza al crear el mundo del servidor. */
	void RunFromCommandLine(UWorld* World, const UWorld::InitializationValues)
	{
		FString Value;
		if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client || !FParse::Value(FCommandLine::Get(), TEXT("TNMineBlast="), Value))
		{
			return;
		}
		// Separado con «_» (1_3_6_6): así no hacen falta comillas en la línea de órdenes.
		TArray<FString> Args;
		Value.ParseIntoArray(Args, TEXT("_"));
		RunBlast(Args, World);
	}

	struct FCommandLineHook
	{
		FCommandLineHook()
		{
			FWorldDelegates::OnPostWorldInitialization.AddStatic(&RunFromCommandLine);
		}
	};

	FCommandLineHook CommandLineHook;

	static FAutoConsoleCommandWithWorldAndArgs BlastCommand(
		TEXT("TN.Beach.Mine.Blast"),
		TEXT("Servidor: TN.Beach.Mine.Blast <jugador> [metros=0] [veces=1] [cada=6] [espera=cada]. Pone una mina a esos metros detrás de la tortuga del jugador (índice en PlayerArray) y la hace saltar; 0 m = en bola, 2-6 m = empujón. Para medir correcciones de red (#18)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunBlast),
		ECVF_Cheat);
}
