#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_BeachElement.h"
#include "World/Beach/TN_BeachTrapCommon.h"
#include "TN_BeachMovingPlatform.generated.h"

class ACharacter;
class ATN_ProcWaterVolume;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UTN_BeachTrapSynthComponent;

/**
 * Plataforma móvil de la playa. Sigue su X local si ya mira al mar (±45°) y si no se gira sola hacia él (X = sentido de
 * la carrera). Dos variantes según la paridad de Spec.Seed:
 *
 * - Balsa (par): un charco hondo dentro de un cráter de arena (lo cavaron los niños y lo llenaron a cubos): el agua es de
 *   verdad (ATN_ProcWaterVolume local en cada máquina: quien cae nada despacio y sale por las orillas, que bajan en
 *   rampa). Encima flota una chancla, una tabla de surf de juguete, un disco volador o la tapa de una fiambrera que va y
 *   viene a lo largo de X de orilla a orilla (con esperas en cada una y meciéndose). Spec.Extent = recorrido de la balsa
 *   (0 → 800 cm; se ajusta para caber en la huella).
 * - Ascensor (impar): una torre cuadrada de arena de molde (4,5 m) con una bandeja o un disco volador colgado de una grúa
 *   de palo de polo que sube y baja por su cara -X (esperas arriba y abajo). Arriba espera una catapulta
 *   (ATN_BeachCatapult) que apunta al mar: desde la torre llega aún más lejos. Spec.Extent = altura de la torre (0 → 450;
 *   250-480, por debajo de los 5 m que meten a la tortuga en el caparazón al caer).
 *
 * La plataforma es una base móvil (colisión convexa con nombre estable por red): quien está encima se mueve con ella sin
 * resbalar. Su movimiento es determinista con el reloj del servidor (TNBeachRideKit::ShuttleAlpha + fase por la semilla):
 * cada máquina la coloca igual, sin replicar posiciones. La catapulta de arriba la crea el servidor y la destruye con la
 * torre.
 */
UCLASS()
class TORTUNABO_API ATN_BeachMovingPlatform : public ATN_BeachElement
{
	GENERATED_BODY()

public:
	ATN_BeachMovingPlatform();

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	bool IsElevator() const { return bElevator; }

	/** Velocidad de la balsa (cm/s) y del ascensor. */
	UPROPERTY(EditAnywhere, Category = "Plataforma", meta = (ClampMin = "50.0"))
	float FerrySpeed = 330.f;

	UPROPERTY(EditAnywhere, Category = "Plataforma", meta = (ClampMin = "50.0"))
	float LiftSpeed = 170.f;

	/** Esperas en cada extremo (s). */
	UPROPERTY(EditAnywhere, Category = "Plataforma", meta = (ClampMin = "0.0"))
	float FerryDwell = 1.6f;

	UPROPERTY(EditAnywhere, Category = "Plataforma", meta = (ClampMin = "0.0"))
	float LiftDwellBottom = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Plataforma", meta = (ClampMin = "0.0"))
	float LiftDwellTop = 1.6f;

	/** El ascensor lleva una catapulta arriba. */
	UPROPERTY(EditAnywhere, Category = "Plataforma")
	bool bCatapultOnTop = true;

protected:
	virtual void BeginPlay() override;
	virtual void ApplySpec() override;

	/** Marco orientado hacia el mar. */
	UPROPERTY(VisibleAnywhere, Category = "Plataforma")
	TObjectPtr<USceneComponent> Frame;

	/** Cráter con el agua, o la torre con la grúa. */
	UPROPERTY(VisibleAnywhere, Category = "Plataforma")
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	UPROPERTY(VisibleAnywhere, Category = "Plataforma")
	TObjectPtr<UProceduralMeshComponent> BaseCollision;

	/** La plataforma que se mueve (base móvil). */
	UPROPERTY(VisibleAnywhere, Category = "Plataforma")
	TObjectPtr<USceneComponent> RideRoot;

	UPROPERTY(VisibleAnywhere, Category = "Plataforma")
	TObjectPtr<UStaticMeshComponent> RideMesh;

	UPROPERTY(VisibleAnywhere, Category = "Plataforma")
	TObjectPtr<UProceduralMeshComponent> RideCollision;

	/** Cuerdas del ascensor (se estiran cada fotograma). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Ropes;

	UPROPERTY(Transient)
	TObjectPtr<UTN_BeachTrapSynthComponent> Voice;

private:
	void BuildFerry(double Fit, uint32 Seed);
	void BuildElevator(double Fit, uint32 Seed);

	/** Coloca la plataforma a la hora Now (todas las máquinas). */
	void PlaceRide(double Now);

	/** Local: cuerdas del ascensor desde la punta de la grúa hasta el borde de la plataforma. */
	void UpdateRopes();

	void SpawnWater();
	void SpawnTopCatapult();

	bool bElevator = false;
	/** Balsa: recorrido del centro de la balsa en X y altura de su cara de arriba. Ascensor: altura de la torre. */
	double Travel = 800.0;
	double RideTopZ = 0.0;
	double RideHalfX = 280.0;
	double RideHalfY = 100.0;
	double RideThick = 40.0;
	bool bRideRound = false;
	double Phase = 0.0;

	// Balsa: charco.
	double WaterZ = 128.0;
	double WaterEdgeR = 620.0;

	// Ascensor: torre y plataforma.
	double TowerHalf = 650.0;
	double TowerH = 450.0;
	double LiftX = -860.0;
	double BottomTopZ = 22.0;
	FVector CraneTipLocal = FVector::ZeroVector;

	/** Última posición (0..1) para los sonidos de salida y llegada. */
	double LastAlpha = -1.0;

	TWeakObjectPtr<ATN_ProcWaterVolume> Water;
	TWeakObjectPtr<ATN_BeachElement> TopCatapult;
	FTNTrapClock Clock;
	FTNTrapBurst Splash;
};
