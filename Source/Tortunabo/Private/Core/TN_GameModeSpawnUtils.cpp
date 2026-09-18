#include "Core/TN_GameModeSpawnUtils.h"
#include "Core/TN_Log.h"
#include "Core/TN_CoopPlayerState.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AActor* TN_PickUnoccupiedPlayerStart(UWorld* World, TArray<AActor*>& PlayerStarts, AController* Player)
{
	// Barajar para evitar siempre el mismo orden
	for (int32 i = PlayerStarts.Num() - 1; i > 0; --i)
	{
		const int32 j = FMath::RandRange(0, i);
		PlayerStarts.Swap(i, j);
	}

	// Primera pasada: buscar un PlayerStart sin ningún pawn cerca (< 200 cm)
	static constexpr float PlayerStartOccupiedRadiusCm = 200.f;
	for (AActor* Start : PlayerStarts)
	{
		bool bOccupied = false;
		for (TActorIterator<APawn> PawnIt(World); PawnIt; ++PawnIt)
		{
			const APawn* P = *PawnIt;
			if (P && P->Controller != Player &&
			    FVector::DistSquared(Start->GetActorLocation(), P->GetActorLocation()) < FMath::Square(PlayerStartOccupiedRadiusCm))
			{
				bOccupied = true;
				break;
			}
		}
		if (!bOccupied)
		{
			return Start;
		}
	}

	return nullptr;
}

void TN_EnsurePlayerSpawned(AGameModeBase* GameMode, APlayerController* PlayerController, TFunctionRef<APlayerStart* ()> FallbackProvider, const TCHAR* LogTag)
{
	if (!GameMode || !GameMode->HasAuthority() || !PlayerController || PlayerController->GetPawn())
	{
		return;
	}

	GameMode->RestartPlayer(PlayerController);
	if (PlayerController->GetPawn())
	{
		return;
	}

	AActor* PlayerStart = GameMode->FindPlayerStart(PlayerController);
	if (!PlayerStart)
	{
		PlayerStart = FallbackProvider();
	}

	if (!PlayerStart)
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[%s] Could not find or create a PlayerStart for %s"), LogTag, *GetNameSafe(PlayerController));
		return;
	}

	APawn* SpawnedPawn = GameMode->SpawnDefaultPawnFor(PlayerController, PlayerStart);
	if (!SpawnedPawn)
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[%s] Failed to spawn default pawn for %s at %s"), LogTag, *GetNameSafe(PlayerController), *GetNameSafe(PlayerStart));
		return;
	}

	PlayerController->Possess(SpawnedPawn);
	GameMode->SetPlayerDefaults(SpawnedPawn);
	UE_LOG(LogTortunabo, Log, TEXT("[%s] Spawned and possessed pawn %s for %s"), LogTag, *GetNameSafe(SpawnedPawn), *GetNameSafe(PlayerController));
}

APlayerStart* TN_EnsureFallbackPlayerStart(UWorld* World, FName SpawnActorName, const TCHAR* LogTag, const TCHAR* MapDescriptor)
{
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		if (APlayerStart* Existing = *It)
		{
			return Existing;
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Name = SpawnActorName;

	APlayerStart* Spawned = World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), FVector(0.f, 0.f, 150.f), FRotator::ZeroRotator, SpawnParams);
	if (Spawned)
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[%s] No PlayerStart found in %s. Spawned fallback PlayerStart at world origin."), LogTag, MapDescriptor);
	}

	return Spawned;
}

int32 TN_CountConnectedCoopPlayers(const AGameStateBase* GameState)
{
	int32 Count = 0;
	for (APlayerState* BasePS : GameState->PlayerArray)
	{
		if (Cast<ATN_CoopPlayerState>(BasePS))
		{
			++Count;
		}
	}
	return Count;
}
