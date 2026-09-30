#pragma once

#include "CoreMinimal.h"
#include "Game/TN_RunGameMode.h"
#include "Game/TN_SurvivalRules.h"
#include "TN_SurvivalGameMode.generated.h"

class ATN_ChunkManager;

/**
 * @brief Modo Supervivencia: niveles cortos del Clásico (LVL_Run, chunks) uno tras otro hasta que queda una tortuga.
 *
 * Se juega en LVL_Run con ?game=Survival (alias en DefaultGame.ini). ATN_ChunkManager pasa al modo por niveles:
 * cada nivel son ChunksPerLevel chunks al azar y el final con la meta, más difíciles cuanto más alto el nivel.
 *  - Quien llega a la meta espera como espectador; cuando todos los vivos han llegado, se genera el siguiente
 *    nivel y vuelven a salir desde los PlayerStart.
 *  - Morir es definitivo (sin DBNO ni rescate; el tótem sí salva) y los muertos espectan.
 *  - En grupo gana la última viva; si las últimas mueren en el mismo nivel, la que murió más cerca de la meta.
 *    En solitario dura hasta que muere.
 * Las reglas están en TN_SurvivalRules.h (tests Tortunabo.Survival).
 */
UCLASS()
class TORTUNABO_API ATN_SurvivalGameMode : public ATN_RunGameMode
{
	GENERATED_BODY()

public:
	ATN_SurvivalGameMode();

	/** @brief Pone el ChunkManager en modo por niveles antes de que los actores hagan BeginPlay (genera el nivel 1). */
	virtual void StartPlay() override;

	virtual void Logout(AController* Exiting) override;

	virtual void MarkPlayerFinished(APlayerController* PlayerController) override;

	virtual void MarkPlayerDead(APlayerController* PlayerController) override;

	/** Nivel que se está jugando (1 el primero). */
	int32 GetCurrentLevel() const { return CurrentLevel; }

protected:
	/** Segundos entre que llega el último vivo y sale el siguiente nivel. */
	UPROPERTY(EditDefaultsOnly, Category = "Survival", meta = (ClampMin = "0.5"))
	float LevelTransitionSeconds = 3.f;

	virtual void OnWaitingTimeout() override;

	/** «?game=» vacío: sin él, el lobby heredaría ?game=Survival y cargaría con este GameMode en vez del HQ (#156). */
	virtual FString GetLobbyTravelOptions() const override { return TEXT("?game="); }

	/** Aplica las reglas de TNSurvivalLogic: seguir, siguiente nivel o fin de partida. */
	virtual void UpdateRoundProgressAndMaybeFinish() override;

private:
	/** Lo que se apunta de cada muerto para desempatar y ordenar. Key = PlayerId. */
	struct FDeathRecord
	{
		int32 Level = 0;
		float Time = 0.f;
		float Remaining = 0.f;
	};
	TMap<int32, FDeathRecord> DeathRecords;

	/** Pawn oculto de quien llegó a la meta: se reutiliza al empezar el siguiente nivel. Key = PlayerId. */
	TMap<int32, TWeakObjectPtr<APawn>> FinishedPawns;

	/** Jugadores que se han ido durante la partida (su PlayerState puede seguir un momento en PlayerArray). */
	TSet<int32> LeftPlayerIds;

	int32 CurrentLevel = 1;
	int32 StartingPlayers = 1;
	bool bMatchOver = false;
	FTimerHandle LevelTransitionTimerHandle;

	ATN_ChunkManager* FindChunkManager() const;

	/** Estado de los jugadores que siguen en la partida, para TNSurvivalLogic. */
	TArray<FTNSurvivalPlayer> GatherPlayers() const;

	/** Siguiente nivel: lo genera y devuelve a los vivos a la salida. */
	void AdvanceLevel();

	/** Fin de partida: puestos en el marcador y Resultados. */
	void FinishSurvival(int32 WinnerId);
};
