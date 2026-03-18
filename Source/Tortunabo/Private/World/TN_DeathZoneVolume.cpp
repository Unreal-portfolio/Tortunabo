#include "World/TN_DeathZoneVolume.h"
#include "Game/TN_RunGameMode.h"
#include "Core/TN_CoopPlayerState.h"
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

	if (PendingDeathTimers.Contains(PC))
	{
		return;
	}

	PendingDeathRemaining.Add(PC, SecondsInsideToDie);
	if (ATN_CoopPlayerState* TNPS = PC->GetPlayerState<ATN_CoopPlayerState>())
	{
		TNPS->DeathZoneTimeRemaining = SecondsInsideToDie;
	}

	FTimerHandle TimerHandle;
	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &ATN_DeathZoneVolume::TickPlayerCountdown, PC);
	GetWorldTimerManager().SetTimer(TimerHandle, TimerDelegate, CountdownTickInterval, true);
	PendingDeathTimers.Add(PC, TimerHandle);
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

	if (FTimerHandle* TimerHandle = PendingDeathTimers.Find(PC))
	{
		GetWorldTimerManager().ClearTimer(*TimerHandle);
		PendingDeathTimers.Remove(PC);
	}

	PendingDeathRemaining.Remove(PC);
	if (ATN_CoopPlayerState* TNPS = PC->GetPlayerState<ATN_CoopPlayerState>())
	{
		TNPS->DeathZoneTimeRemaining = -1.f;
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
		RunGameMode->MarkPlayerDead(PlayerController);
	}

	PendingDeathTimers.Remove(PlayerController);
	PendingDeathRemaining.Remove(PlayerController);
	if (ATN_CoopPlayerState* TNPS = PlayerController->GetPlayerState<ATN_CoopPlayerState>())
	{
		TNPS->DeathZoneTimeRemaining = -1.f;
	}
}

void ATN_DeathZoneVolume::TickPlayerCountdown(APlayerController* PlayerController)
{
	if (!HasAuthority() || !PlayerController)
	{
		return;
	}

	float* Remaining = PendingDeathRemaining.Find(PlayerController);
	if (!Remaining)
	{
		return;
	}

	*Remaining = FMath::Max(0.f, *Remaining - CountdownTickInterval);
	if (ATN_CoopPlayerState* TNPS = PlayerController->GetPlayerState<ATN_CoopPlayerState>())
	{
		TNPS->DeathZoneTimeRemaining = *Remaining;
	}

	if (*Remaining <= KINDA_SMALL_NUMBER)
	{
		if (FTimerHandle* TimerHandle = PendingDeathTimers.Find(PlayerController))
		{
			GetWorldTimerManager().ClearTimer(*TimerHandle);
		}
		HandlePlayerDeath(PlayerController);
	}
}

ATN_RunGameMode* ATN_DeathZoneVolume::ResolveRunGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<ATN_RunGameMode>() : nullptr;
}

