// Estado replicado del Rally en el mapa generado del cooperativo (#291): además del de ATN_RallyGameState, la generación
// del mapa con la que se hizo la pista y la dificultad. Cada cliente construye su pista con el camino de su propio
// generador (que genera el mismo mapa con la semilla replicada) en cuanto lo tiene, y avisa al servidor para que el
// semáforo no empiece antes.
#pragma once

#include "CoreMinimal.h"
#include "Rally/TN_RallyGameState.h"
#include "World/ProcMap/TN_ProcMapEnums.h"
#include "TN_ProcRallyGameState.generated.h"

class ATN_ProcMapGenerator;

UCLASS()
class TORTUNABO_API ATN_ProcRallyGameState : public ATN_RallyGameState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Generación del mapa (ATN_ProcMapGenerator) con la que el servidor hizo la pista; 0 = aún ninguna. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ProcGeneration, Category = "Rally")
	int32 ProcGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rally")
	ETNProcDifficulty ProcDifficulty = ETNProcDifficulty::Normal;

	/** Semilla del mapa (para el menú de pausa y para repetirlo con ?ProcSeed=). */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rally")
	int32 MapSeed = 0;

	/** Largo de la pista (cm), para el HUD. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rally")
	float TrackLengthCm = 0.f;

	/** Servidor: construye la pista con el mapa que acaba de generar Generator. */
	ATN_RallyTrack* BuildTrackFromMap(ATN_ProcMapGenerator& Generator);

	/** El generador del nivel (nullptr si aún no ha llegado en un cliente). */
	ATN_ProcMapGenerator* FindGenerator() const;

	/** Generación con la que está hecha la pista de esta máquina (0 = sin pista). */
	int32 GetBuiltTrackGeneration() const { return TrackGeneration; }

private:
	UFUNCTION()
	void OnRep_ProcGeneration();

	/** Cliente: si su generador ya tiene la generación que pide el servidor, construye la pista y avisa. */
	void TryBuildClientTrack();
	void HandleMapGenerated(int32 Generation);

	int32 TrackGeneration = 0;
	TWeakObjectPtr<ATN_ProcMapGenerator> BoundGenerator;
	FDelegateHandle MapGeneratedHandle;
	FTimerHandle RetryHandle;
};
