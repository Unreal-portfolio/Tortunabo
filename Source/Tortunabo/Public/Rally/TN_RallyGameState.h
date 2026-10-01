// Estado replicado de la carrera del Rally: fase, horas del servidor (semáforo, cierre y resultados), variante y puestos.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "TN_RallyGameState.generated.h"

class APawn;
class APlayerState;
class ATN_RallyTrack;

UENUM(BlueprintType)
enum class ETNRallyPhase : uint8
{
	/** Llegan las tortugas y se sientan; motores cortados. */
	Warmup,
	/** Semáforo de 3 s; salir antes corta el motor 1 s tras el verde. */
	Countdown,
	Racing,
	/** El primero ya ha llegado: quedan 20 s para los demás. */
	Finishing,
	/** Tabla de resultados 15 s y carrera nueva en el mismo mapa. */
	Results
};

/** Un registro por buggy (equipo): ocupantes, progreso y puesto. Lo rellena el servidor a 5 Hz, ordenado por puesto. */
USTRUCT(BlueprintType)
struct FTNRallyStanding
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Rally")
	int32 TeamIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Rally")
	TObjectPtr<APawn> Vehicle = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Rally")
	TObjectPtr<APlayerState> Driver = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Rally")
	TObjectPtr<APlayerState> Gunner = nullptr;

	/** 1 = primero. */
	UPROPERTY(BlueprintReadOnly, Category = "Rally")
	int32 Place = 0;

	/** Vuelta en curso (1..Laps); 0 antes de cruzar la salida. */
	UPROPERTY(BlueprintReadOnly, Category = "Rally")
	int32 Lap = 0;

	/** Siguiente puerta que tiene que cruzar (índice en la pista). */
	UPROPERTY(BlueprintReadOnly, Category = "Rally")
	int32 NextGate = 0;

	/** Segundos desde la salida al cruzar la meta. */
	UPROPERTY(BlueprintReadOnly, Category = "Rally")
	float FinishSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Rally")
	bool bFinished = false;

	/** Sin ocupantes en plena carrera. */
	UPROPERTY(BlueprintReadOnly, Category = "Rally")
	bool bRetired = false;

	UPROPERTY(BlueprintReadOnly, Category = "Rally")
	bool bWrongWay = false;

	UPROPERTY(BlueprintReadOnly, Category = "Rally")
	bool bBot = false;

	/** Hora del servidor a la que acaba la espera de la reaparición en curso (0 = no reaparece). */
	UPROPERTY(BlueprintReadOnly, Category = "Rally")
	float RespawnEndServerTime = 0.f;

	/** Puntos de copa por el puesto (10-8-6-5-4-3-2-1; 0 sin llegar). Definitivos en Results. */
	UPROPERTY(BlueprintReadOnly, Category = "Rally")
	int32 Points = 0;
};

DECLARE_MULTICAST_DELEGATE(FTNRallyTrackReady);

UCLASS()
class TORTUNABO_API ATN_RallyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rally")
	ETNRallyPhase Phase = ETNRallyPhase::Warmup;

	/** Hora del servidor del verde del semáforo (StartServerTime). */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rally")
	float StartServerTime = 0.f;

	/** Fin de la fase en curso (calentamiento, cierre tras el primero o resultados); 0 = sin cuenta. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rally")
	float PhaseEndServerTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rally")
	int32 Laps = 1;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rally")
	int32 NumGates = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rally")
	bool bCircuit = false;

	/** Variante del mapa: los clientes cargan la misma en su ATN_MapVariantLoader y construyen su pista. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Variant, Category = "Rally")
	FName Variant;

	/** Ordenado por puesto. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rally")
	TArray<FTNRallyStanding> Standings;

	/** Registro del buggy en que va este jugador (conductora o artillera); nullptr si no va en ninguno. */
	const FTNRallyStanding* FindStandingForPlayer(const APlayerState* Player) const;
	const FTNRallyStanding* FindStandingForVehicle(const APawn* Vehicle) const;

	/** Pista de esta máquina (la construye el GameMode en el servidor y OnRep_Variant en los clientes). */
	ATN_RallyTrack* GetTrack() const { return Track; }

	/**
	 * Fija Variant en el ATN_MapVariantLoader del nivel (lo crea si no hay), recarga el terreno si hace falta y construye
	 * la pista (ATN_RallyTrack del nivel o una nueva). Lo usan el GameMode (servidor) y OnRep_Variant (clientes).
	 */
	ATN_RallyTrack* PrepareTrack(FName InVariant);

	/** Se dispara al tener pista en esta máquina. */
	FTNRallyTrackReady OnTrackReady;

	/** Texto de una línea con la fase y los puestos (TN.Rally.Status). */
	FString DescribeStatus() const;

protected:
	UFUNCTION()
	void OnRep_Variant();

private:
	UPROPERTY(Transient)
	TObjectPtr<ATN_RallyTrack> Track;
};
