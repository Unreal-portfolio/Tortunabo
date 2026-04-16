#include "World/TN_TotemInteractable.h"
#include "Player/TortugaCharacter.h"
#include "Core/TN_CoopPlayerState.h"
#include "Game/TN_RunGameMode.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"

ATN_TotemInteractable::ATN_TotemInteractable()
{
	PromptText           = FText::FromString(TEXT("Activar Tótem"));
	InteractionDistance  = 250.f;
	CooldownSeconds      = 5.f;
}

void ATN_TotemInteractable::Interact(APawn* Interactor)
{
	if (!HasAuthority() || !Interactor || !CanInteract(Interactor)) { return; }

	// ── Buscar jugadores eliminados ───────────────────────────────────────────
	TArray<APlayerController*> DeadPlayers;
	for (TActorIterator<APlayerController> It(GetWorld()); It; ++It)
	{
		APlayerController* PC = *It;
		if (!PC) { continue; }
		ATN_CoopPlayerState* PS = PC->GetPlayerState<ATN_CoopPlayerState>();
		if (PS && PS->bIsEliminated)
		{
			DeadPlayers.Add(PC);
		}
	}

	if (DeadPlayers.Num() == 0)
	{
		MulticastOnNoTarget();
		return;
	}

	// ── Seleccionar aleatoriamente ────────────────────────────────────────────
	const int32 Idx = FMath::RandRange(0, DeadPlayers.Num() - 1);
	APlayerController* TargetPC = DeadPlayers[Idx];

	// ── Revivir via RunGameMode ────────────────────────────────────────────────
	ATN_RunGameMode* GM = Cast<ATN_RunGameMode>(GetWorld()->GetAuthGameMode());
	if (!GM)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Totem] No ATN_RunGameMode found — cannot revive."));
		return;
	}

	GM->RevivePlayer(TargetPC);

	// ── Teleportar al revivido cerca del activador ────────────────────────────
	// RevivePlayer ya hizo Possess y situó el pawn en zona segura de chunks.
	// Sobreescribimos la posición para que aparezca al lado del activador.
	APawn* RevivedPawn = TargetPC->GetPawn();
	if (RevivedPawn)
	{
		const FVector ActivatorLoc = Interactor->GetActorLocation();
		// Offset lateral para no aparecer encima del activador
		const FVector RightOffset  = Interactor->GetActorRightVector() * ReviveSpawnOffset;
		const FVector SpawnLoc     = ActivatorLoc + RightOffset;
		RevivedPawn->TeleportTo(SpawnLoc, Interactor->GetActorRotation());
	}

	// ── Feedback visual ────────────────────────────────────────────────────────
	ATortugaCharacter* RevivedChar  = Cast<ATortugaCharacter>(RevivedPawn);
	ATortugaCharacter* ActivatorChar = Cast<ATortugaCharacter>(Interactor);
	MulticastOnActivated(RevivedChar, ActivatorChar);

	// ── Destruir o cooldown ───────────────────────────────────────────────────
	if (bDestroyAfterUse)
	{
		Destroy();
		return;
	}

	// Si no se destruye, delegar a la base para gestionar cooldown
	Super::Interact(Interactor);
}

void ATN_TotemInteractable::MulticastOnActivated_Implementation(ATortugaCharacter* RevivedCharacter,
	ATortugaCharacter* Activator)
{
	OnTotemActivated(RevivedCharacter, Activator);
}

void ATN_TotemInteractable::MulticastOnNoTarget_Implementation()
{
	OnTotemNoTarget();
}
