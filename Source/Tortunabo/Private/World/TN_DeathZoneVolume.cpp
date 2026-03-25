#include "World/TN_DeathZoneVolume.h"
#include "Game/TN_RunGameMode.h"
#include "Core/TN_CoopPlayerState.h"
#include "Player/TortugaCharacter.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

ATN_DeathZoneVolume::ATN_DeathZoneVolume()
{
	OnActorBeginOverlap.AddDynamic(this, &ATN_DeathZoneVolume::OnZoneBeginOverlap);
	OnActorEndOverlap.AddDynamic(this, &ATN_DeathZoneVolume::OnZoneEndOverlap);
}

void ATN_DeathZoneVolume::OnZoneBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(OtherActor);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		return;
	}

	ATN_RunGameMode* RunGameMode = ResolveRunGameMode();
	if (!RunGameMode)
	{
		return;
	}

	if (bDestroyOnlyDuringRun && RunGameMode->GetMatchState() != MatchState::InProgress)
	{
		return;
	}

	if (PendingDeathRemaining.Contains(PC))
	{
		return;
	}

	PendingDeathRemaining.Add(PC, SecondsInsideToDie);
	if (ATN_CoopPlayerState* TNPS = PC->GetPlayerState<ATN_CoopPlayerState>())
	{
		TNPS->DeathZoneTimeRemaining = SecondsInsideToDie;
	}

	// Arrancar el timer compartido si no está activo
	if (!GetWorldTimerManager().IsTimerActive(SharedCountdownTimerHandle))
	{
		GetWorldTimerManager().SetTimer(SharedCountdownTimerHandle, this,
			&ATN_DeathZoneVolume::TickAllCountdowns, CountdownTickInterval, true);
	}
}

void ATN_DeathZoneVolume::OnZoneEndOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(OtherActor);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		return;
	}

	PendingDeathRemaining.Remove(PC);
	if (ATN_CoopPlayerState* TNPS = PC->GetPlayerState<ATN_CoopPlayerState>())
	{
		TNPS->DeathZoneTimeRemaining = -1.f;
	}

	// Detener el timer compartido si no queda nadie
	if (PendingDeathRemaining.Num() == 0)
	{
		GetWorldTimerManager().ClearTimer(SharedCountdownTimerHandle);
	}
}

void ATN_DeathZoneVolume::HandlePlayerDeath(APlayerController* PlayerController)
{
	if (!HasAuthority() || !PlayerController)
	{
		return;
	}

	if (ATN_RunGameMode* RunGameMode = ResolveRunGameMode())
	{
		// Muerte instantánea — sin DBNO/bleedout
		RunGameMode->MarkPlayerDead(PlayerController);
	}

	PendingDeathRemaining.Remove(PlayerController);
	if (ATN_CoopPlayerState* TNPS = PlayerController->GetPlayerState<ATN_CoopPlayerState>())
	{
		TNPS->DeathZoneTimeRemaining = -1.f;
	}

	// Detener el timer compartido si no queda nadie
	if (PendingDeathRemaining.Num() == 0)
	{
		GetWorldTimerManager().ClearTimer(SharedCountdownTimerHandle);
	}
}

void ATN_DeathZoneVolume::TickAllCountdowns()
{
	if (!HasAuthority())
	{
		return;
	}

	// Iterar sobre una copia de las claves para poder modificar el map durante el loop
	TArray<TWeakObjectPtr<APlayerController>> Keys;
	PendingDeathRemaining.GetKeys(Keys);

	for (const TWeakObjectPtr<APlayerController>& WeakPC : Keys)
	{
		APlayerController* PC = WeakPC.Get();
		if (!PC)
		{
			PendingDeathRemaining.Remove(WeakPC);
			continue;
		}

		float* Remaining = PendingDeathRemaining.Find(WeakPC);
		if (!Remaining) { continue; }

		// ── Pausar el countdown mientras el jugador está derribado (DBNO) ──────
		// Si el jugador fue noqueado dentro de la zona y un compañero lo revive,
		// sin esta pausa el contador llega a 0 y lo mata inmediatamente al revivir.
		// Al estar derribado reseteamos a SecondsInsideToDie → cuando se levante
		// tendrá el tiempo completo para salir.
		if (const ATortugaCharacter* Char = Cast<ATortugaCharacter>(PC->GetPawn()))
		{
			if (Char->IsKnockedDown())
			{
				*Remaining = SecondsInsideToDie;
				if (ATN_CoopPlayerState* TNPS = PC->GetPlayerState<ATN_CoopPlayerState>())
				{
					TNPS->DeathZoneTimeRemaining = SecondsInsideToDie;
				}
				continue;
			}
		}

		*Remaining = FMath::Max(0.f, *Remaining - CountdownTickInterval);
		if (ATN_CoopPlayerState* TNPS = PC->GetPlayerState<ATN_CoopPlayerState>())
		{
			TNPS->DeathZoneTimeRemaining = *Remaining;
		}

		if (*Remaining <= KINDA_SMALL_NUMBER)
		{
			HandlePlayerDeath(PC);
		}
	}

	// Si tras procesar todos no queda nadie, detener el timer
	if (PendingDeathRemaining.Num() == 0)
	{
		GetWorldTimerManager().ClearTimer(SharedCountdownTimerHandle);
	}
}

void ATN_DeathZoneVolume::ResetPlayerTimer(APlayerController* PC)
{
	if (!HasAuthority() || !PC)
	{
		return;
	}

	float* Remaining = PendingDeathRemaining.Find(PC);
	if (Remaining)
	{
		*Remaining = SecondsInsideToDie;
		if (ATN_CoopPlayerState* TNPS = PC->GetPlayerState<ATN_CoopPlayerState>())
		{
			TNPS->DeathZoneTimeRemaining = SecondsInsideToDie;
		}
	}
}

ATN_RunGameMode* ATN_DeathZoneVolume::ResolveRunGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<ATN_RunGameMode>() : nullptr;
}

