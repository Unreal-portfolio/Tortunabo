#include "World/TN_FinishLineVolume.h"
#include "Game/TN_RunGameMode.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ATN_FinishLineVolume::ATN_FinishLineVolume()
{
	OnActorBeginOverlap.AddDynamic(this, &ATN_FinishLineVolume::OnFinishBeginOverlap);
}

void ATN_FinishLineVolume::OnFinishBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}

	if (const APawn* Pawn = Cast<APawn>(OtherActor))
	{
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			if (ATN_RunGameMode* RunGM = ResolveRunGameMode())
			{
				RunGM->MarkPlayerFinished(PC);
			}
		}
	}
}

ATN_RunGameMode* ATN_FinishLineVolume::ResolveRunGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<ATN_RunGameMode>() : nullptr;
}

