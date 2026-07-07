#include "Lobby/TN_LobbyReadyZone.h"
#include "Core/TN_Log.h"
#include "Lobby/TN_HQGameMode.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ATN_LobbyReadyZone::ATN_LobbyReadyZone()
{
	OnActorBeginOverlap.AddDynamic(this, &ATN_LobbyReadyZone::OnZoneBeginOverlap);
	OnActorEndOverlap.AddDynamic(this, &ATN_LobbyReadyZone::OnZoneEndOverlap);
}

void ATN_LobbyReadyZone::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	// Verify the game mode is reachable — common config mistake
	if (!ResolveHQGameMode())
	{
		UE_LOG(LogTortunabo, Error, TEXT("[LobbyReadyZone] No se encontro TN_HQGameMode en el mundo. "
			"Asegurate de que LVL_HQ tiene GameMode Override = TN_HQGameMode (o BP derivado)."));
	}

	// Safety scan: handle pawns that spawned inside the zone before BeginPlay
	TArray<AActor*> Overlapping;
	GetOverlappingActors(Overlapping, APawn::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		HandlePawnEnter(Cast<APawn>(Actor));
	}
}

void ATN_LobbyReadyZone::HandlePawnEnter(APawn* Pawn)
{
	if (!Pawn)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC)
	{
		return;
	}

	if (ATN_HQGameMode* HQGM = ResolveHQGameMode())
	{
		UE_LOG(LogTortunabo, Log, TEXT("[LobbyReadyZone] Jugador %s ENTRO en la zona."), *GetNameSafe(PC));
		HQGM->SetPlayerReadyState(PC, true);
	}
}

void ATN_LobbyReadyZone::HandlePawnExit(APawn* Pawn)
{
	if (!Pawn)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC)
	{
		return;
	}

	if (ATN_HQGameMode* HQGM = ResolveHQGameMode())
	{
		UE_LOG(LogTortunabo, Log, TEXT("[LobbyReadyZone] Jugador %s SALIO de la zona."), *GetNameSafe(PC));
		HQGM->SetPlayerReadyState(PC, false);
	}
}

void ATN_LobbyReadyZone::OnZoneBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}

	HandlePawnEnter(Cast<APawn>(OtherActor));
}

void ATN_LobbyReadyZone::OnZoneEndOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}

	HandlePawnExit(Cast<APawn>(OtherActor));
}

ATN_HQGameMode* ATN_LobbyReadyZone::ResolveHQGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<ATN_HQGameMode>() : nullptr;
}
