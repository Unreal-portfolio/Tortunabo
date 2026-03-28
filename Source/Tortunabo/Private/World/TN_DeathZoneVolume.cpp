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

	// No iniciar el countdown si el jugador tiene inmunidad post-revive activa.
	// Esto evita que un jugador recién revivido muera instantáneamente si cae
	// dentro de una death zone antes de que expire su periodo de gracia.
	if (RunGameMode->IsPlayerReviveImmune(PC))
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

		// Pausar el countdown mientras el jugador está noquedado por puffer-fish.
		// bIsKnockedDown=true → bIsAlive=true, no puede moverse; matarlo sería injusto.
		if (const ATortugaCharacter* Turtle = Cast<ATortugaCharacter>(PC->GetPawn()))
		{
			if (Turtle->IsKnockedDown()) { continue; }
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

void ATN_DeathZoneVolume::ForceCheckPlayer(APlayerController* PC)
{
	if (!HasAuthority() || !PC || !TriggerBox)
	{
		return;
	}

	// Ya está siendo rastreado → no duplicar
	if (PendingDeathRemaining.Contains(PC))
	{
		return;
	}

	// Verificar que el jugador está vivo
	ATN_CoopPlayerState* TNPS = PC->GetPlayerState<ATN_CoopPlayerState>();
	if (!TNPS || !TNPS->bIsAlive)
	{
		return;
	}

	// Verificar restricción de fase de juego
	if (bDestroyOnlyDuringRun)
	{
		ATN_RunGameMode* RunGameMode = ResolveRunGameMode();
		if (!RunGameMode || RunGameMode->GetMatchState() != MatchState::InProgress)
		{
			return;
		}
	}

	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		return;
	}

	// ── Comprobación geométrica: ¿está el pawn dentro del TriggerBox? ──
	// No depende de la lista de overlaps del motor de físicas, que puede estar
	// desactualizada tras togglear SetActorEnableCollision.
	const FTransform BoxTransform = TriggerBox->GetComponentTransform();
	const FVector LocalPos = BoxTransform.InverseTransformPosition(Pawn->GetActorLocation());
	const FVector Extent = TriggerBox->GetUnscaledBoxExtent();

	const bool bInside = FMath::Abs(LocalPos.X) <= Extent.X
	                  && FMath::Abs(LocalPos.Y) <= Extent.Y
	                  && FMath::Abs(LocalPos.Z) <= Extent.Z;

	if (!bInside)
	{
		return;
	}

	// ── Iniciar countdown ──
	PendingDeathRemaining.Add(PC, SecondsInsideToDie);
	TNPS->DeathZoneTimeRemaining = SecondsInsideToDie;

	if (!GetWorldTimerManager().IsTimerActive(SharedCountdownTimerHandle))
	{
		GetWorldTimerManager().SetTimer(SharedCountdownTimerHandle, this,
			&ATN_DeathZoneVolume::TickAllCountdowns, CountdownTickInterval, true);
	}

	UE_LOG(LogTemp, Log, TEXT("[DeathZone] ForceCheckPlayer: '%s' está dentro de '%s' — countdown reiniciado (%.1fs)"),
		*GetNameSafe(PC), *GetName(), SecondsInsideToDie);
}

