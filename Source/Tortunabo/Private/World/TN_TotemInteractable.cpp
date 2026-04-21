#include "World/TN_TotemInteractable.h"
#include "Player/TortugaCharacter.h"
#include "Core/TN_CoopPlayerState.h"
#include "Game/TN_RunGameMode.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"

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
	// GetPawn() puede ser null si el spawn falló (no había spawn point, etc.)
	APawn* RevivedPawn = TargetPC->GetPawn();
	if (!RevivedPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Totem] RevivePlayer succeeded but GetPawn() returned null for '%s'"),
			*GetNameSafe(TargetPC));
		MulticastOnNoTarget();
		return;
	}

	{
		const FVector ActivatorLoc = Interactor->GetActorLocation();
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
	if (SoundActivate)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SoundActivate, GetActorLocation());
	}
	if (VFXActivate)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), VFXActivate, GetActorLocation());
	}
	OnTotemActivated(RevivedCharacter, Activator);
}

void ATN_TotemInteractable::MulticastOnNoTarget_Implementation()
{
	if (SoundNoTarget)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SoundNoTarget, GetActorLocation());
	}
	OnTotemNoTarget();
}
