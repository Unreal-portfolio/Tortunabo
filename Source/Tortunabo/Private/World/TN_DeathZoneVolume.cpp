#include "World/TN_DeathZoneVolume.h"
#include "Game/TN_RunGameMode.h"
#include "Core/TN_CoopPlayerState.h"
#include "Player/TortugaCharacter.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

ATN_DeathZoneVolume::ATN_DeathZoneVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);
	TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 200.f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->SetHiddenInGame(true);

#if WITH_EDITORONLY_DATA
	TriggerBox->ShapeColor = FColor::Red;
	TriggerBox->SetLineThickness(2.f);
#endif

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ATN_DeathZoneVolume::OnBoxBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ATN_DeathZoneVolume::OnBoxEndOverlap);
}

void ATN_DeathZoneVolume::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
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

void ATN_DeathZoneVolume::OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
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
