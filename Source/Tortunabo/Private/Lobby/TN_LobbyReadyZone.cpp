#include "Lobby/TN_LobbyReadyZone.h"
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
}

void ATN_LobbyReadyZone::OnZoneBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}

	if (const APawn* Pawn = Cast<APawn>(OtherActor))
	{
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			if (ATN_HQGameMode* HQGM = ResolveHQGameMode())
			{
				HQGM->SetPlayerReadyState(PC, true);
			}
		}
	}
}

void ATN_LobbyReadyZone::OnZoneEndOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}

	if (const APawn* Pawn = Cast<APawn>(OtherActor))
	{
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			if (ATN_HQGameMode* HQGM = ResolveHQGameMode())
			{
				HQGM->SetPlayerReadyState(PC, false);
			}
		}
	}
}

ATN_HQGameMode* ATN_LobbyReadyZone::ResolveHQGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<ATN_HQGameMode>() : nullptr;
}

