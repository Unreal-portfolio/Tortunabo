// Acciones y contextos de Enhanced Input del buggy, creados en C++ en tiempo de ejecución (sin .uasset de input).
// Controles de Docs/Rally_MVP.md: teclado y ratón, y mando.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TN_BuggyInput.generated.h"

class APlayerController;
class UInputAction;
class UInputMappingContext;

UCLASS(Transient)
class TORTUNABO_API UTN_BuggyInputSet : public UObject
{
	GENERATED_BODY()

public:
	/** Prioridad de los contextos del buggy: por encima de los del personaje, que pueden seguir puestos. */
	static constexpr int32 ContextPriority = 10;

	/** Crea las acciones y los dos contextos (conductora y artillera) con Outer como dueño. */
	static UTN_BuggyInputSet* Create(UObject* Outer);

	/** Pone Context en el subsistema de Enhanced Input del jugador local de PC (no hace nada si no es local). */
	static void AddContext(const APlayerController* PC, const UInputMappingContext* Context);
	static void RemoveContext(const APlayerController* PC, const UInputMappingContext* Context);

	// Conductora
	UPROPERTY() TObjectPtr<UInputAction> Throttle;
	UPROPERTY() TObjectPtr<UInputAction> Brake;
	UPROPERTY() TObjectPtr<UInputAction> Steer;
	UPROPERTY() TObjectPtr<UInputAction> Handbrake;
	/** Atrás (Q · X): mientras se mantiene, la conductora sola dispara hacia atrás. */
	UPROPERTY() TObjectPtr<UInputAction> FireBack;

	// Las dos
	UPROPERTY() TObjectPtr<UInputAction> SelfRight;
	UPROPERTY() TObjectPtr<UInputAction> FireCoco;
	UPROPERTY() TObjectPtr<UInputAction> FireSpecial;

	// Artillera
	/** Apuntar con el ratón (delta por frame). */
	UPROPERTY() TObjectPtr<UInputAction> AimMouse;
	/** Apuntar con el stick derecho (velocidad, -1..1 por eje). */
	UPROPERTY() TObjectPtr<UInputAction> AimStick;

	UPROPERTY() TObjectPtr<UInputMappingContext> DriverContext;
	UPROPERTY() TObjectPtr<UInputMappingContext> GunnerContext;
};
