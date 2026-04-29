#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_ButtonGroupManager.generated.h"

class ATN_ButtonInteractable;

/**
 * Struct que representa una acción de transform sobre un actor objetivo.
 * Configurable completamente desde el Editor / Blueprint.
 */
USTRUCT(BlueprintType)
struct FTN_TransformAction
{
	GENERATED_BODY()

	/** Actor al que se aplicará el transform. Asignar por eyedropper en el nivel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TransformAction")
	TObjectPtr<AActor> TargetActor = nullptr;

	/**
	 * Tag del actor objetivo (fallback cuando TargetActor es null).
	 * Útil para actores spawneados desde chunks que no están disponibles en el editor.
	 * La primera vez que se aplica la acción se hace un world scan y el resultado se
	 * cachea en TargetActor para los usos posteriores.
	 * Prioridad: TargetActor > TargetActorTag.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TransformAction")
	FName TargetActorTag = NAME_None;

	/** Desplazamiento de posición a añadir a la posición ACTUAL del actor (relativo). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TransformAction")
	FVector LocationOffset = FVector::ZeroVector;

	/** Rotación adicional (Euler degrees) añadida al estado actual del actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TransformAction")
	FRotator RotationOffset = FRotator::ZeroRotator;

	/** Duración de la transición de movimiento (segundos). 0 = instantáneo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TransformAction", meta = (ClampMin = "0.0"))
	float TransitionDuration = 1.0f;

	/**
	 * Aplica un array de acciones. bForward=true añade offsets, false los revierte.
	 * Punto centralizado para evitar duplicar la lógica en cada Manager.
	 * World se usa para resolver TargetActorTag lazily si TargetActor es null.
	 */
	static void ApplyAll(TArray<FTN_TransformAction>& Actions, bool bForward = true, UWorld* World = nullptr);
};

/**
 * Gestor de grupo de botones (#24).
 *
 * Monitorea un array de ATN_ButtonInteractable.
 * Cuando TODOS los botones del grupo están activados (bIsActivated=true):
 *   → Aplica las TriggerActions al array de actores objetivo.
 *
 * Autoridad: toda la lógica en el servidor. Las acciones se propagan
 * mediante replicación de los actores objetivo (si son replicados).
 *
 * Uso en Editor:
 *   1. Colocar BP_ButtonGroupManager en el nivel.
 *   2. En la propiedad ManagedButtons, añadir referencias a los botones del puzzle.
 *   3. En TriggerActions, añadir los actores y los offsets de transform.
 *   4. Opcionalmente implementar OnAllButtonsActivated en BP para VFX.
 */
UCLASS(Blueprintable)
class TORTUNABO_API ATN_ButtonGroupManager : public AActor
{
	GENERATED_BODY()

public:
	ATN_ButtonGroupManager();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** Botones que este gestor monitorea. Asignar en el nivel por eyedropper. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ButtonGroup")
	TArray<TObjectPtr<ATN_ButtonInteractable>> ManagedButtons;

	/**
	 * Tags de actores para descubrir botones automáticamente en BeginPlay.
	 * Útil cuando los botones están en chunks spawneados en runtime y no se pueden
	 * asignar por eyedropper. El manager también escanea periódicamente si llegan
	 * tarde (los botones de chunks se registran solos vía ManagerTag).
	 * Se combinan con ManagedButtons — no son excluyentes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ButtonGroup")
	TArray<FName> ManagedButtonTags;

	/**
	 * Acciones aplicadas cuando TODOS los botones están activados.
	 * Cada acción mueve/rota un actor objetivo.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ButtonGroup")
	TArray<FTN_TransformAction> TriggerActions;

	/**
	 * Si true, una vez activado el grupo no se puede desactivar
	 * aunque los botones se togleen de vuelta.
	 * Si false, desactivar cualquier botón revierte las acciones.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ButtonGroup")
	bool bOneShot = true;

	/**
	 * Nº mínimo de botones activados para disparar. -1 (default) = TODOS.
	 * Ej: 4 botones, Threshold=2 → basta con 2 activados para aplicar las acciones.
	 * Se clampea a [1, ManagedButtons.Num()].
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ButtonGroup")
	int32 TriggerThreshold = -1;

	/** Llamado en TODAS las máquinas cuando todos los botones están activados. */
	UFUNCTION(BlueprintImplementableEvent, Category = "ButtonGroup")
	void OnAllButtonsActivated();

public:
	/**
	 * Registra un botón con este gestor en runtime (llamado por ATN_ButtonInteractable
	 * cuando tiene ManagerTag configurado). Idempotente: ignorar duplicados.
	 * Solo tiene efecto en el servidor.
	 */
	void RegisterButton(ATN_ButtonInteractable* Button);

	/** Llamado en TODAS las máquinas cuando el grupo se desactiva (solo si bOneShot=false). */
	UFUNCTION(BlueprintImplementableEvent, Category = "ButtonGroup")
	void OnGroupDeactivated();

private:
	bool bTriggered = false;
	bool bCurrentlyActivated = false;

	void OnButtonActivationChanged(ATN_ButtonInteractable* Button, bool bActivated);
	void CheckAndTrigger();
	void ApplyTriggerActions(bool bForward);

	/** Scan diferido (un tick después de BeginPlay) para registrar botones por tag. */
	void DeferredTagButtonScan();

	int32 GetEffectiveThreshold() const;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastNotifyActivated();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastNotifyDeactivated();
};
