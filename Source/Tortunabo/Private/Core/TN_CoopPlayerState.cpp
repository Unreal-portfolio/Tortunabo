#include "Core/TN_CoopPlayerState.h"
#include "Player/TortugaCharacter.h"
#include "Net/UnrealNetwork.h"

ATN_CoopPlayerState::ATN_CoopPlayerState()
{
}

void ATN_CoopPlayerState::OnRep_EquippedHelmetId()
{
	// Actualiza el mesh del casco en el personaje local cuando el servidor replica el cambio.
	// GetPawn() puede ser null si el PlayerState se recibe antes de la posesión del pawn.
	if (ATortugaCharacter* TurtleChar = Cast<ATortugaCharacter>(GetPawn()))
	{
		TurtleChar->UpdateHelmetMesh(EquippedHelmetId);
	}
}

void ATN_CoopPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATN_CoopPlayerState, bIsInReadyZone);
	DOREPLIFETIME(ATN_CoopPlayerState, bHasFinishedRun);
	DOREPLIFETIME(ATN_CoopPlayerState, bIsAlive);
	DOREPLIFETIME(ATN_CoopPlayerState, bIsDBNO);
	DOREPLIFETIME_CONDITION(ATN_CoopPlayerState, DBNOBleedoutTimeRemaining, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ATN_CoopPlayerState, DeathZoneTimeRemaining, COND_OwnerOnly);
	DOREPLIFETIME(ATN_CoopPlayerState, EquippedHelmetId);
	DOREPLIFETIME(ATN_CoopPlayerState, FinishTimeSeconds);
	DOREPLIFETIME(ATN_CoopPlayerState, FinishRank);
}

