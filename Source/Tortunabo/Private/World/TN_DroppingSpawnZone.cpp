#include "World/TN_DroppingSpawnZone.h"
#include "Core/TN_Log.h"
#include "World/TN_SeagullDroppingActor.h"
#include "Player/TortugaCharacter.h"

ATN_DroppingSpawnZone::ATN_DroppingSpawnZone()
{
	// Defaults específicos de la caca (la base define el resto).
	SpawnInterval = 8.f;
	InitialDelay  = 3.f;
	MaxConcurrent = 2;
}

void ATN_DroppingSpawnZone::TrySpawn()
{
	if (!DroppingClass) { return; }
	if (GetActiveCount() >= MaxConcurrent) { return; }

	TArray<ATortugaCharacter*> Candidates;
	GetLivingPlayersInside(Candidates);
	if (Candidates.Num() == 0) { return; }

	ATortugaCharacter* Target = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];

	// La caca cae sobre la XY del jugador, pero si éste está en el borde del volumen
	// podría impactar fuera. Clampeamos al AABB para garantizar que el impacto siempre
	// cae dentro de la zona dueña.
	const FVector ClampedGround = ClampXYToVolume(Target->GetActorLocation());

	if (ATN_SeagullDroppingActor* Dropping = ATN_SeagullDroppingActor::SpawnDroppingAtLocation(
		this, DroppingClass, ClampedGround))
	{
		RegisterActive(Dropping);

		UE_LOG(LogTortunabo, Verbose, TEXT("[DroppingSpawnZone] '%s': caca spawneada sobre '%s'."),
			*GetName(), *GetNameSafe(Target));
	}
}
