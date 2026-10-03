// Rally en el mapa generado del cooperativo (#291): LVL_ProcMap?game=Rally (alias «Rally» en DefaultEngine.ini). Se elige
// en el lobby como los demás modos (ETNProcGameMode::Rally, ATN_HQGameMode::BeginMatchTravel). El servidor genera el mapa
// del cooperativo con su perfil y la dificultad del lobby, pero con el camino hecho para el buggy (TNProcMap::FGenParams::
// bDrivable), y la pista del Rally sale del camino principal (TNProcRally::BuildTrackFromGenerator): puertas cada 250 m con
// la regla del 60 %, salida en el claro inicial y meta en la playa final. Al acabar, todas vuelven al lobby.
//
// Pruebas sin lobby: open LVL_ProcMap?game=Rally?ProcDifficulty=Easy|Normal|Hard?ProcSeed=N?Bots=N?Seats=1|2. Con
// ?AutoStart, ?RaceTimeout=S y ?Races=N, carreras solo de la IA en un servidor sin jugadoras (como en LVL_Rally).
#pragma once

#include "CoreMinimal.h"
#include "Rally/TN_RallyGameMode.h"
#include "World/ProcMap/TN_ProcMapEnums.h"
#include "TN_ProcRallyGameMode.generated.h"

class ATN_ProcMapGenerator;
class ATN_ProcRallyGameState;
class UTN_ProcMapSettings;

UCLASS()
class TORTUNABO_API ATN_ProcRallyGameMode : public ATN_RallyGameMode
{
	GENERATED_BODY()

public:
	ATN_ProcRallyGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void Logout(AController* Exiting) override;

	/** Una jugadora tiene ya en su máquina la pista de esta generación del mapa (ATN_RallyPlayerController). */
	void NotifyClientTrackReady(APlayerController* Player, int32 Generation);

	/** Acaba la carrera y lleva a todas al lobby del que salieron (menú de pausa del anfitrión). */
	void ReturnToLobbyNow();

	UFUNCTION(BlueprintPure, Category = "Rally")
	ETNProcDifficulty GetProcDifficulty() const { return Difficulty; }

	/** Buggies mínimos en la parrilla: si faltan jugadoras, los completan bots (sin ?Bots= en la URL). */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Bots", meta = (ClampMin = "0", ClampMax = "8"))
	int32 MinBuggies = 4;

	/** Velocidad máxima de los bots en recta por dificultad (km/h): fácil, normal y difícil. */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Bots")
	FVector BotMaxSpeedKmh = FVector(78.f, 88.f, 98.f);

	/** Probabilidad de que un bot gaste su munición especial al disparar, por dificultad. */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Bots")
	FVector BotSpecialFireChance = FVector(0.15f, 0.3f, 0.45f);

	/** Ajustes del mapa si el generador del nivel no trae los suyos. */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Mapa")
	TSoftObjectPtr<UTN_ProcMapSettings> MapSettings;

	/** Semilla fija (0 = aleatoria); ?ProcSeed= manda. */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Mapa")
	int32 FixedSeed = 0;

	/** Tope de espera a que el suelo de la parrilla tenga colisión (s): luego se sienta igual. */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Mapa")
	float GroundReadyTimeoutSeconds = 20.f;

protected:
	virtual ATN_RallyTrack* BuildRaceTrack(ATN_RallyGameState& RallyState) override;
	virtual bool IsReadyToSeat() const override;
	virtual bool AreClientsReady() const override;
	virtual int32 GetExpectedHumans() const override;
	virtual void FinishSession() override;
	virtual void ConfigureBot(ATN_RallyAIController& Pilot, int32 BotIndex) override;
	virtual int32 ResolveBotCount() const override;

private:
	ATN_ProcMapGenerator* EnsureGenerator();
	ATN_ProcRallyGameState* GetProcRallyState() const;

	UPROPERTY(Transient)
	TObjectPtr<ATN_ProcMapGenerator> Generator;

	ETNProcDifficulty Difficulty = ETNProcDifficulty::Normal;
	int32 UrlSeed = 0;
	/** Hora (s del mundo) en que empezó a esperar el suelo de la parrilla. */
	double GroundWaitStart = -1.0;
	bool bReturning = false;
	mutable bool bLoggedLineProbe = false;

	/** Última generación del mapa con la pista hecha en cada máquina cliente. */
	TMap<TWeakObjectPtr<APlayerController>, int32> ClientTrackGeneration;
};
