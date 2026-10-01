// Ajuste del buggy del Rally. Port de UHYBuggyData (HellYeah) sin carga ni boost: los valores por defecto son los de
// BUGGY_DATA (Tools/Unreal/build_data_assets.py de HellYeah) y los usa ATN_Buggy si no se le asigna un asset.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TN_BuggyData.generated.h"

UCLASS(BlueprintType)
class TORTUNABO_API UTN_BuggyData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Par máximo del motor (N·m). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buggy")
	float MaxTorque = 850.f;

	/** Régimen máximo (rpm): fija la punta, unos 110 km/h con la relación final 2,0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buggy")
	float MaxRPM = 3400.f;

	/** Relación final de la transmisión: con 2,0 llega a 60 km/h en 3 s y deja derrapar con gas en curva. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buggy")
	float FinalDriveRatio = 2.0f;

	/** Multiplicador de fricción de las ruedas delanteras. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buggy")
	float FrontFriction = 3.0f;

	/** Multiplicador de fricción de las ruedas traseras. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buggy")
	float RearFriction = 2.6f;

	/** Fricción trasera con el freno de mano: más baja = más derrape. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buggy")
	float HandbrakeRearFriction = 1.4f;

	/** Contravolante añadido (fracción de la dirección) al llegar a MaxAssistAngleDeg de deriva. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buggy")
	float CounterSteerAssist = 0.8f;

	/** Deriva (grados) a la que la asistencia llega a su máximo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buggy")
	float MaxAssistAngleDeg = 35.f;

	/** Segundos volcado antes de poder enderezar pulsando R (o Y). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enderezado")
	float SelfRightManualDelay = 0.5f;

	/** Segundos volcado a los que el servidor lo endereza solo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enderezado")
	float SelfRightAutoDelay = 4.f;

	/** Cuánto sube el buggy (cm) al enderezarlo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enderezado")
	float SelfRightLiftCm = 100.f;

	/** Segundos manteniendo R (o Y) para pedir la reaparición. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enderezado")
	float RespawnHoldSeconds = 1.5f;

	/** Subida mínima del suelo bajo la rueda (cm) en un frame para contar como escalón. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suspension")
	float BumpMinStepCm = 4.f;

	/** Fracción de la velocidad de subida teórica que pasa al chasis. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suspension")
	float BumpKickScale = 0.15f;

	/** Tope del golpe (cm/s hacia arriba en la rueda). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suspension")
	float BumpKickMax = 200.f;

	/** Dirección máxima del bamboleo del coco (fracción del volante) y su frecuencia (Hz). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impactos")
	float WobbleAmplitude = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impactos")
	float WobbleFrequency = 5.f;

	/** Ganancia del frenado en el charco: deceleración (cm/s²) por cada cm/s de más sobre la velocidad tope. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impactos")
	float PuddleBrakeGain = 3.f;
};
