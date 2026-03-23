#include "World/TN_RescuePickup.h"
#include "Game/TN_RunGameMode.h"
#include "Core/TN_CoopPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

ATN_RescuePickup::ATN_RescuePickup()
{
	bReplicates = true;
	PromptText = FText::FromString(TEXT("Rescatar"));
	InteractionDistance = 300.f;
}

void ATN_RescuePickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATN_RescuePickup, DeadPlayerId);
}

void ATN_RescuePickup::SetDeadPlayerId(int32 InPlayerId)
{
	DeadPlayerId = InPlayerId;
}

bool ATN_RescuePickup::CanInteract(APawn* Interactor) const
{
	if (!Super::CanInteract(Interactor))
	{
		return false;
	}

	// No puede rescatarse a sí mismo ni interactuar si el jugador muerto ya no existe
	if (DeadPlayerId < 0)
	{
		return false;
	}

	// No dejar que el propio jugador muerto (si de alguna forma tiene pawn) se auto-rescue
	if (Interactor)
	{
		if (APlayerController* InteractorPC = Cast<APlayerController>(Interactor->GetController()))
		{
			if (APlayerState* PS = InteractorPC->GetPlayerState<APlayerState>())
			{
				if (PS->GetPlayerId() == DeadPlayerId)
				{
					return false;
				}
			}
		}
	}

	return true;
}

void ATN_RescuePickup::Interact(APawn* Interactor)
{
	if (!HasAuthority())
	{
		return;
	}

	// Buscar el PlayerController del jugador muerto por PlayerId
	APlayerController* DeadPC = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) { continue; }
		if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
		{
			if (PS->GetPlayerId() == DeadPlayerId)
			{
				DeadPC = PC;
				break;
			}
		}
	}

	if (!DeadPC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RescuePickup] Could not find PlayerController for DeadPlayerId=%d"), DeadPlayerId);
		Destroy();
		return;
	}

	// Llamar a RevivePlayer del GameMode
	if (ATN_RunGameMode* RunGM = Cast<ATN_RunGameMode>(GetWorld()->GetAuthGameMode()))
	{
		// Teletransportar el pawn del muerto a la posición del pickup ANTES de revivir
		if (APawn* DeadPawn = DeadPC->GetPawn())
		{
			DeadPawn->SetActorLocation(GetActorLocation() + FVector(0.f, 0.f, 50.f));
		}

		RunGM->RevivePlayer(DeadPC);

		UE_LOG(LogTemp, Log, TEXT("[RescuePickup] Revived player (Id=%d) at (%.0f,%.0f,%.0f)"),
			DeadPlayerId, GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[RescuePickup] No TN_RunGameMode found — cannot revive"));
	}

	// Llamar OnInteracted para hooks BP
	OnInteracted(Interactor);

	Destroy();
}

