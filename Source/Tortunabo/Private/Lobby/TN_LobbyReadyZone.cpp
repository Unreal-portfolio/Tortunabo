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

	if (HasAuthority())
	{
		// Ensure collision is set up correctly for overlap events
		SetActorEnableCollision(true);
		
		// Force update of collision to ensure reliable overlap events
		if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(GetRootComponent()))
		{
			RootPrim->SetGenerateOverlapEvents(true);
			RootPrim->SetCollisionEnabled(ECC_Pawn);
			
			// Force a collision update
			RootPrim->UpdateOverlaps();
		}

		UE_LOG(LogTemp, Log, TEXT("[Lobby] Ready zone initialized with collision and overlap events enabled"));
	}
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
				UE_LOG(LogTemp, Log, TEXT("[Lobby] Player entered ready zone: %s"), *GetNameSafe(PC));
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
				UE_LOG(LogTemp, Log, TEXT("[Lobby] Player left ready zone: %s"), *GetNameSafe(PC));
				HQGM->SetPlayerReadyState(PC, false);
			}
		}
	}
}

ATN_HQGameMode* ATN_LobbyReadyZone::ResolveHQGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<ATN_HQGameMode>() : nullptr;
}

