// Comandos de consola del buggy para la prueba de humo (fuera de Shipping). Se pueden encadenar en -ExecCmds en el
// mismo frame: cada uno espera lo que necesita con un temporizador del mundo.
//   TN.Rally.SpawnBuggy                       (servidor) crea un buggy delante del jugador y lo posee
//   TN.Rally.DebugDrive <gas> <giro> <s> [espera]  conduce con SetAIDriveInput y registra el desplazamiento
//   TN.Rally.SpawnTarget [cm] [espera]       (servidor) buggy vacío delante como blanco
//   TN.Rally.DebugFireAll [espera] [atrás]            dispara una vez cada munición (coco, alga, burbuja, mortero, tinta), 1 s entre una y otra
//   TN.Rally.DebugQuitAfter <s>               cierra el juego pasados s segundos

#include "Vehicles/TN_Buggy.h"
#include "Vehicles/TN_BuggyMath.h"
#include "Vehicles/TN_BuggyTurretComponent.h"
#include "Vehicles/TN_RallyTurretLogic.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"

#if !UE_BUILD_SHIPPING

namespace TNBuggyDebug
{
	float FloatArg(const TArray<FString>& Args, int32 Index, float Default)
	{
		return Args.IsValidIndex(Index) ? FCString::Atof(*Args[Index]) : Default;
	}

	/** El buggy del primer jugador o, si no conduce ninguno, el primero del mundo. */
	ATN_Buggy* FindBuggy(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			if (ATN_Buggy* Mine = Cast<ATN_Buggy>(PC->GetPawn()))
			{
				return Mine;
			}
		}
		for (TActorIterator<ATN_Buggy> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	/** Llama a Action pasados Seconds (0 = en el siguiente frame), si el mundo sigue vivo. */
	void After(UWorld* World, float Seconds, TFunction<void(UWorld*)> Action)
	{
		if (!World)
		{
			return;
		}
		TWeakObjectPtr<UWorld> WeakWorld(World);
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakWorld, Action]()
		{
			if (UWorld* Alive = WeakWorld.Get())
			{
				Action(Alive);
			}
		}), FMath::Max(Seconds, 0.01f), false);
	}

	void SpawnBuggy(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (!PC || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogTNBuggy, Warning, TEXT("[Humo] TN.Rally.SpawnBuggy: solo en el servidor y con un jugador"));
			return;
		}
		FVector ViewLocation;
		FRotator ViewRotation;
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
		const APawn* Pawn = PC->GetPawn();
		const FVector Base = Pawn ? Pawn->GetActorLocation() : ViewLocation;
		const FRotator Yaw(0.f, Pawn ? Pawn->GetActorRotation().Yaw : ViewRotation.Yaw, 0.f);
		const FVector Location = Base + Yaw.Vector() * 800.f + FVector(0.f, 0.f, 150.f);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		ATN_Buggy* Buggy = World->SpawnActor<ATN_Buggy>(ATN_Buggy::StaticClass(), Location, Yaw, Params);
		if (!Buggy)
		{
			UE_LOG(LogTNBuggy, Error, TEXT("[Humo] no se ha podido crear el buggy"));
			return;
		}
		Buggy->SetRallyTeamIndex(0);
		const bool bSeated = Buggy->SeatController(PC, ETNRallySeat::Driver);
		UE_LOG(LogTNBuggy, Log, TEXT("[Humo] buggy %s creado en (%.0f, %.0f, %.0f); conductora sentada=%d"), *Buggy->GetName(),
			Location.X, Location.Y, Location.Z, bSeated ? 1 : 0);
	}

	/** Buggy sin nadie (equipo 1) DistanceCm delante del buggy del jugador: blanco para los disparos. */
	void SpawnTarget(UWorld* World, float DistanceCm)
	{
		ATN_Buggy* Mine = FindBuggy(World);
		if (!Mine || !Mine->HasAuthority())
		{
			UE_LOG(LogTNBuggy, Warning, TEXT("[Humo] TN.Rally.SpawnTarget: no hay buggy en el servidor"));
			return;
		}
		// Delante si hay suelo; si no (borde del mapa), detrás o a un lado.
		const FRotator Yaw(0.f, Mine->GetActorRotation().Yaw, 0.f);
		const FVector Directions[] = { Yaw.Vector(), -Yaw.Vector(), FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), -FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y) };
		FVector Location = Mine->GetActorLocation() + Directions[0] * DistanceCm;
		for (const FVector& Dir : Directions)
		{
			const FVector Probe = Mine->GetActorLocation() + Dir * DistanceCm;
			FHitResult Ground;
			FCollisionQueryParams Query(FName(TEXT("TNRallySpawnTarget")), false, Mine);
			if (World->LineTraceSingleByChannel(Ground, Probe + FVector(0.f, 0.f, 500.f), Probe - FVector(0.f, 0.f, 2000.f), ECC_WorldStatic, Query))
			{
				Location = Ground.ImpactPoint;
				break;
			}
		}
		Location.Z += 150.f;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		ATN_Buggy* Target = World->SpawnActor<ATN_Buggy>(ATN_Buggy::StaticClass(), Location, Yaw, Params);
		if (Target)
		{
			Target->SetRallyTeamIndex(1);
		}
		UE_LOG(LogTNBuggy, Log, TEXT("[Humo] blanco %s a %.0f cm"), *GetNameSafe(Target), DistanceCm);
	}

	void Drive(UWorld* World, float Throttle, float Steer, float Seconds)
	{
		ATN_Buggy* Buggy = FindBuggy(World);
		if (!Buggy || !Buggy->HasAuthority())
		{
			UE_LOG(LogTNBuggy, Warning, TEXT("[Humo] TN.Rally.DebugDrive: no hay buggy en el servidor"));
			return;
		}
		const FVector Start = Buggy->GetActorLocation();
		UE_LOG(LogTNBuggy, Log, TEXT("[Humo] conduce %s gas=%.2f giro=%.2f durante %.1f s desde (%.0f, %.0f, %.0f)"), *Buggy->GetName(),
			Throttle, Steer, Seconds, Start.X, Start.Y, Start.Z);
		// Las entradas se mantienen cada frame (a 30 Hz) hasta el final, como haría el piloto IA.
		TWeakObjectPtr<ATN_Buggy> WeakBuggy(Buggy);
		const int32 Steps = FMath::Max(1, FMath::RoundToInt(Seconds * 30.f));
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			After(World, Step / 30.f, [WeakBuggy, Throttle, Steer](UWorld*)
			{
				if (ATN_Buggy* Alive = WeakBuggy.Get())
				{
					Alive->SetAIDriveInput(Throttle, 0.f, Steer, false);
				}
			});
		}
		After(World, Seconds, [WeakBuggy, Start, Seconds](UWorld*)
		{
			ATN_Buggy* Alive = WeakBuggy.Get();
			if (!Alive)
			{
				UE_LOG(LogTNBuggy, Error, TEXT("[Humo] el buggy ha desaparecido durante la prueba"));
				return;
			}
			// Frena 2 s y luego suelta todo con el freno de mano: con bReverseAsBrake, seguir frenando parado mete la
			// marcha atrás, y el freno de mano a toda velocidad lo hace trompear.
			Alive->SetAIDriveInput(0.f, 1.f, 0.f, false);
			After(Alive->GetWorld(), 2.f, [WeakBuggy](UWorld*)
			{
				if (ATN_Buggy* Stopped = WeakBuggy.Get())
				{
					Stopped->SetAIDriveInput(0.f, 0.f, 0.f, true);
				}
			});
			const FVector End = Alive->GetActorLocation();
			const float Moved = FVector::Dist2D(Start, End);
			UE_LOG(LogTNBuggy, Log, TEXT("[Humo] desplazamiento=%.0f cm en %.1f s velocidad=%.0f cm/s volcado=%d arribaZ=%.2f"),
				Moved, Seconds, Alive->GetForwardSpeedCms(), Alive->IsFlipped() ? 1 : 0, Alive->GetActorUpVector().Z);
		});
	}

	void FireAll(UWorld* World, bool bBackward)
	{
		const ETNRallyAmmo Order[] = { ETNRallyAmmo::Coco, ETNRallyAmmo::Alga, ETNRallyAmmo::Burbuja, ETNRallyAmmo::Mortero, ETNRallyAmmo::Tinta };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Order); ++Index)
		{
			const ETNRallyAmmo Ammo = Order[Index];
			After(World, static_cast<float>(Index), [Ammo, bBackward](UWorld* Alive)
			{
				ATN_Buggy* Buggy = FindBuggy(Alive);
				if (!Buggy || !Buggy->HasAuthority() || !Buggy->GetTurret())
				{
					UE_LOG(LogTNBuggy, Warning, TEXT("[Humo] TN.Rally.DebugFireAll: no hay buggy en el servidor"));
					return;
				}
				const bool bSpecial = TNRallyTurret::IsSpecial(Ammo);
				if (bSpecial)
				{
					Buggy->GiveSpecialAmmo(Ammo, TNRallyTurret::SpecFor(Ammo).BoxCharges);
				}
				for (TActorIterator<ATN_Buggy> It(Alive); It; ++It)
				{
					if (*It != Buggy)
					{
						UE_LOG(LogTNBuggy, Log, TEXT("[Humo] %s a %.0f cm del tirador"), *It->GetName(), FVector::Dist(It->GetActorLocation(), Buggy->GetActorLocation()));
					}
				}
				const int32 ProjectilesBefore = Alive->GetActorCount();
				Buggy->DriverFireAuto(bSpecial, bBackward);
				UE_LOG(LogTNBuggy, Log, TEXT("[Humo] disparo de %s: actores %d -> %d, calor=%.2f, especial=%s x%d"), *UEnum::GetValueAsString(Ammo),
					ProjectilesBefore, Alive->GetActorCount(), Buggy->GetTurret()->GetHeat01(),
					*UEnum::GetValueAsString(Buggy->GetSpecialAmmo()), Buggy->GetTurret()->GetSpecialCharges());
			});
		}
	}

	FAutoConsoleCommandWithWorldAndArgs CmdSpawnBuggy(TEXT("TN.Rally.SpawnBuggy"),
		TEXT("Rally (servidor): crea un buggy 8 m delante del jugador y lo posee como conductora."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			SpawnBuggy(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdSpawnTarget(TEXT("TN.Rally.SpawnTarget"),
		TEXT("Rally (servidor): TN.Rally.SpawnTarget [distancia cm = 2500] [espera]: crea un buggy vacío delante del buggy del jugador como blanco."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const float Distance = FloatArg(Args, 0, 2500.f);
			After(World, FloatArg(Args, 1, 0.f), [Distance](UWorld* Alive) { SpawnTarget(Alive, Distance); });
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdDebugDrive(TEXT("TN.Rally.DebugDrive"),
		TEXT("Rally (servidor): TN.Rally.DebugDrive <gas 0..1> <giro -1..1> <segundos> [espera]: conduce el buggy del jugador con SetAIDriveInput y registra el desplazamiento (LogTNBuggy [Humo])."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const float Throttle = FloatArg(Args, 0, 1.f);
			const float Steer = FloatArg(Args, 1, 0.f);
			const float Seconds = FloatArg(Args, 2, 5.f);
			// Espera por defecto de 1 s: el buggy recién creado cae y se asienta antes de arrancar.
			After(World, FloatArg(Args, 3, 1.f), [Throttle, Steer, Seconds](UWorld* Alive) { Drive(Alive, Throttle, Steer, Seconds); });
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdDebugFireAll(TEXT("TN.Rally.DebugFireAll"),
		TEXT("Rally (servidor): TN.Rally.DebugFireAll [espera] [atrás 0|1]: el buggy del jugador dispara una vez cada munición, 1 s entre una y otra, con el apuntado automático de la conductora sola."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const bool bBackward = FloatArg(Args, 1, 0.f) != 0.f;
			After(World, FloatArg(Args, 0, 0.f), [bBackward](UWorld* Alive) { FireAll(Alive, bBackward); });
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdDebugQuitAfter(TEXT("TN.Rally.DebugQuitAfter"),
		TEXT("Rally: TN.Rally.DebugQuitAfter <segundos>: cierra el juego pasado ese tiempo (para encadenar la prueba de humo en -ExecCmds)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			After(World, FloatArg(Args, 0, 10.f), [](UWorld* Alive)
			{
				UE_LOG(LogTNBuggy, Log, TEXT("[Humo] fin"));
				UKismetSystemLibrary::QuitGame(Alive, Alive->GetFirstPlayerController(), EQuitPreference::Quit, false);
			});
		}));
}

#endif
