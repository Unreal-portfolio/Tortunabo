#pragma once

#include "CoreMinimal.h"
#include "World/Beach/TN_BeachElement.h"
#include "World/Beach/TN_BeachTrapCommon.h"
#include "TN_BeachCatapult.generated.h"

class ACharacter;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UTN_BeachTrapSynthComponent;
class UTN_PlaygroundSynthComponent;

/**
 * Catapulta de playa hecha con cosas de niños: una cuchara gigante (de plástico, de madera o dos palos de polo atados con
 * un vasito de yogur por cazo) apoyada como un balancín sobre un tapón de garrafa encima de una piedra (o sobre una
 * piedra sola), con un cubito de arena mojada por contrapeso en el extremo del mango y un palo de polo de pie que lo
 * sujeta en alto. El cazo descansa en la arena (-X) y lanza hacia su X local si ya mira al mar (±45°: el reparto le
 * deja libre el arco de salto por ahí); si no, se gira sola hacia el mar (TNBeachRideKit::LaunchYawInActor).
 *
 * Con SizeScale = 1: brazo de ~11 m, fulcro a 2,5 m, cazo de ~3 x 2,2 m. Se entra en el cazo andando (borde bajo) o se
 * sube por el mango hasta el cubito.
 *
 * Reglas (servidor): una tortuga libre en el cazo la arma: aviso de WarnSeconds (el palo que sujeta tiembla y cruje cada
 * vez más, el banderín parpadea); si el cazo se queda vacío, se desarma. Si otra tortuga cae de un salto sobre el cubito,
 * dispara al momento. Al disparar, el palo sale volando, el cubito cae, la cuchara da la vuelta (golpe y rebote contra la
 * arena) y lanza como bolas de caparazón (UTN_ShellComponent: vuelan, rebotan y ruedan; salen solas al pararse) a las
 * que estén en el cazo (LaunchSpeed a LaunchPitch grados hacia el mar, ±DeviationDeg de desvío al azar) y, más flojo, a
 * las que estén en el mango. Luego recarga (ReloadSeconds): la cuchara vuelve despacio con un carraca, el palo se pone
 * de pie y el banderín pasa de rojo a verde.
 *
 * Red: el servidor decide (ArmedAt y FiredAt, horas del servidor replicadas) y lanza las bolas (la caja física se
 * replica sola: sin predicción ni correcciones). Cada máquina anima la cuchara, el palo, el banderín, el polvo y los
 * sonidos desde esas horas con el reloj del servidor suavizado. La colisión del brazo se apaga durante el golpe para
 * que no toque las bolas que salen.
 */
UCLASS()
class TORTUNABO_API ATN_BeachCatapult : public ATN_BeachElement
{
	GENERATED_BODY()

public:
	ATN_BeachCatapult();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Lista para disparar (en cualquier máquina). */
	bool IsLoaded(double ServerTime) const;

	/** Aviso desde que alguien se sube al cazo hasta que dispara. */
	UPROPERTY(EditAnywhere, Category = "Catapulta", meta = (ClampMin = "0.0"))
	float WarnSeconds = 1.0f;

	/** Recarga desde que dispara hasta que vuelve a estar lista. */
	UPROPERTY(EditAnywhere, Category = "Catapulta", meta = (ClampMin = "1.0"))
	float ReloadSeconds = 4.6f;

	/** Velocidad de la bola lanzada desde el cazo (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Catapulta", meta = (ClampMin = "0.0"))
	float LaunchSpeed = 2300.f;

	/** Elevación del lanzamiento (grados). */
	UPROPERTY(EditAnywhere, Category = "Catapulta", meta = (ClampMin = "10.0", ClampMax = "80.0"))
	float LaunchPitch = 44.f;

	/** Desvío máximo a cada lado (grados, al azar en cada tiro). */
	UPROPERTY(EditAnywhere, Category = "Catapulta", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float DeviationDeg = 12.f;

	/** Fracción de la velocidad para las que estén en el mango (no en el cazo). */
	UPROPERTY(EditAnywhere, Category = "Catapulta", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HandleLaunchFraction = 0.55f;

protected:
	virtual void BeginPlay() override;
	virtual void ApplySpec() override;

	UFUNCTION()
	void OnRep_Shot();

	/** Hora del servidor en que se armó (aviso en marcha; < 0 = no). */
	UPROPERTY(ReplicatedUsing = OnRep_Shot)
	float ArmedAt = -1.f;

	/** Hora del servidor del último disparo (< 0 = ninguno). */
	UPROPERTY(ReplicatedUsing = OnRep_Shot)
	float FiredAt = -1.f;

	/** Marco orientado hacia el mar. */
	UPROPERTY(VisibleAnywhere, Category = "Catapulta")
	TObjectPtr<USceneComponent> Frame;

	/** Piedra, tapón y banderines. */
	UPROPERTY(VisibleAnywhere, Category = "Catapulta")
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	UPROPERTY(VisibleAnywhere, Category = "Catapulta")
	TObjectPtr<UProceduralMeshComponent> BaseCollision;

	/** Eje del balancín, sobre el tapón (gira en cabeceo). */
	UPROPERTY(VisibleAnywhere, Category = "Catapulta")
	TObjectPtr<USceneComponent> ArmPivot;

	/** Cuchara con el cubito (espacio del eje: X hacia el cubito). */
	UPROPERTY(VisibleAnywhere, Category = "Catapulta")
	TObjectPtr<UStaticMeshComponent> ArmMesh;

	/** Cazo, mango y cubito: base móvil con nombre estable por red. */
	UPROPERTY(VisibleAnywhere, Category = "Catapulta")
	TObjectPtr<UProceduralMeshComponent> ArmCollision;

	/** Palo de polo que sujeta el mango en alto (cae al disparar). */
	UPROPERTY(VisibleAnywhere, Category = "Catapulta")
	TObjectPtr<USceneComponent> PropPivot;

	UPROPERTY(VisibleAnywhere, Category = "Catapulta")
	TObjectPtr<UStaticMeshComponent> PropMesh;

	/** Banderín verde (lista) y rojo (armada o recargando). */
	UPROPERTY(VisibleAnywhere, Category = "Catapulta")
	TObjectPtr<UStaticMeshComponent> FlagGreen;

	UPROPERTY(VisibleAnywhere, Category = "Catapulta")
	TObjectPtr<UStaticMeshComponent> FlagRed;

	UPROPERTY(Transient)
	TObjectPtr<UTN_BeachTrapSynthComponent> Voice;

	UPROPERTY(Transient)
	TObjectPtr<UTN_PlaygroundSynthComponent> Toy;

private:
	struct FRider
	{
		float LastVz = 0.f;
		bool bOnBucket = false;
	};

	void ServerTick(double Now);

	/** Servidor: dispara y lanza a quien esté en el cazo y en el mango. */
	void Fire(double Now);

	/** Cabeceo del brazo (grados; + = cubito arriba) a la hora Now. */
	double ArmPitchAt(double Now) const;

	/** Dónde está una tortuga sobre el brazo: 0 = fuera, 1 = cazo, 2 = mango, 3 = encima del cubito. */
	int32 WhereOnArm(const ACharacter* Character) const;

	FVector LaunchVelocity(float Fraction) const;
	void TickVisuals(double Now, float DeltaSeconds);

	// Medidas (cm; espacio del eje del brazo, X hacia el cubito).
	double LongArm = 750.0;
	double ShortArm = 350.0;
	double PivotZ = 250.0;
	double RestDeg = 16.0;
	double FiredDeg = -40.0;
	double BowlLength = 300.0;
	double BowlHalfWidth = 110.0;
	double BowlDepth = 38.0;
	double ArmThick = 30.0;
	double BucketRadius = 88.0;
	double BucketHeight = 70.0;
	double FrameYawDeg = 0.0;

	double LastVisualNow = -1.0;
	double LastRatchetAt = -1.0;
	float CreakTimer = 0.f;
	bool bArmCollisionOn = true;
	double EmptySince = -1.0;

	TMap<TWeakObjectPtr<ACharacter>, FRider> Riders;
	FTNTrapClock Clock;
	FTNTrapBurst Dust;
	FTNTrapBurst Chips;
	FTNTrapPopText Pop;
};
