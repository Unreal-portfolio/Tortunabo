#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "TN_BeachCameraShake.generated.h"

class APlayerController;

/**
 * Temblor de cámara de la playa (en el proyecto no había camera shakes): un modificador de cámara que cada jugador local
 * añade a su PlayerCameraManager la primera vez que tiembla. Dos entradas que se suman:
 *  - Kick: un golpe que se apaga solo (mazazo del cangrejo, pisotón, quad que pasa rozando).
 *  - Rumble: un temblor sostenido mientras alguien lo refresque cada fotograma (quad que se acerca, tormenta encima);
 *    si nadie lo refresca, se apaga en unas décimas.
 * El temblor es ruido suave en el giro (cabeceo, guiñada y alabeo) y un poco en la posición, más fuerte con el
 * cuadrado de la intensidad. Solo visual y local: cada máquina lo lanza a partir del estado replicado o de un multicast.
 */
UCLASS()
class TORTUNABO_API UTN_BeachCameraShake : public UCameraModifier
{
	GENERATED_BODY()

public:
	UTN_BeachCameraShake(const FObjectInitializer& ObjectInitializer);

	virtual bool ModifyCamera(float DeltaTime, struct FMinimalViewInfo& InOutPOV) override;

	/**
	 * Golpe para los jugadores locales cerca de Where: Strength (0..1) entero hasta InnerRadius y apagándose hasta
	 * OuterRadius (cm, medidos desde su tortuga o, si no tiene, desde su cámara).
	 */
	static void Kick(const UObject* WorldContext, const FVector& Where, float Strength, float InnerRadius, float OuterRadius);

	/** Temblor sostenido (llamar cada fotograma mientras dure), con la misma caída con la distancia que Kick. */
	static void Rumble(const UObject* WorldContext, const FVector& Where, float Strength, float InnerRadius, float OuterRadius);

	/** El modificador de ese jugador (lo crea si no lo tiene). Null sin PlayerCameraManager. */
	static UTN_BeachCameraShake* GetFor(APlayerController* PC);

	void AddTrauma(float Amount);
	void SetRumble(float Level);

private:
	/** Golpes acumulados (0..1): baja solo. */
	float Trauma = 0.f;
	/** Temblor sostenido pedido en el último fotograma y el que se ve (suavizado). */
	float RumbleTarget = 0.f;
	float RumbleShown = 0.f;
	float RumbleStamp = -1.f;
	/** Reloj propio (s). */
	float Clock = 0.f;

	/** Fuerza para un jugador local según la distancia (0 fuera de OuterRadius). */
	static float Falloff(const APlayerController* PC, const FVector& Where, float InnerRadius, float OuterRadius);
};
