#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_ButtonGroupManager.generated.h"

class ATN_ButtonInteractable;
class UNiagaraSystem;
class USoundBase;

/** Reglas del grupo de botones como lógica pura (las usa ATN_ButtonGroupManager; tests en Tortunabo.World.ButtonGroup). */
namespace TNButtonGroupRules
{
	enum class EGroupChange : uint8
	{
		None,
		Activate,
		Deactivate
	};

	/**
	 * @brief Qué le pasa al grupo con ActiveCount botones pulsados de Required.
	 * @param bActive  Si el grupo ya está activado.
	 * @param bOneShot Una vez activado no se desactiva aunque se suelten botones.
	 */
	inline EGroupChange Decide(int32 ActiveCount, int32 Required, bool bActive, bool bOneShot)
	{
		const bool bMet = ActiveCount >= Required;
		if (bMet && !bActive)
		{
			return EGroupChange::Activate;
		}
		if (!bMet && bActive && !bOneShot)
		{
			return EGroupChange::Deactivate;
		}
		return EGroupChange::None;
	}

	/**
	 * @brief ¿Tiene un cliente que mover él mismo el objetivo de una acción? Solo si el servidor no le replica su
	 *        movimiento (p. ej. una puerta del nivel que no replica): si lo replica, moverlo también en el cliente lo
	 *        desplazaría dos veces (los desplazamientos son relativos).
	 */
	inline bool ClientMovesTarget(bool bTargetReplicates, bool bTargetReplicatesMovement)
	{
		return !(bTargetReplicates && bTargetReplicatesMovement);
	}
}

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
	 * bOnlyClientMoved: solo los objetivos cuyo movimiento no replica el servidor (TNButtonGroupRules::ClientMovesTarget).
	 */
	static void ApplyAll(TArray<FTN_TransformAction>& Actions, bool bForward = true, UWorld* World = nullptr, bool bOnlyClientMoved = false);
};

/**
 * Gestor de grupo de botones (#24).
 *
 * Monitorea un array de ATN_ButtonInteractable.
 * Cuando TODOS los botones del grupo están activados (bIsActivated=true):
 *   → Aplica las TriggerActions al array de actores objetivo.
 *
 * Autoridad: toda la lógica en el servidor. El estado del grupo (bGroupActive) se replica con OnRep y es la fuente de
 * verdad: quien entra tarde, o para quien el gestor no era relevante al resolverse el puzzle, lo ve resuelto. Las
 * acciones las aplica el servidor; los objetivos que replican su movimiento llegan así a los clientes, y los que no (una
 * puerta del nivel sin réplica) los mueve cada cliente en su OnRep. El sonido y el efecto del momento van aparte, por un
 * multicast no fiable (solo quien está en ese momento).
 *
 * Uso en Editor:
 *   1. Colocar BP_ButtonGroupManager en el nivel.
 *   2. En la propiedad ManagedButtons, añadir referencias a los botones del puzzle.
 *   3. En TriggerActions, añadir los actores y los offsets de transform.
 *   4. Opcionalmente, en los valores por defecto, ActivatedSound, ActivatedVFX y DeactivatedSound.
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

	/** Sonido al activarse el grupo (solo quien está en ese momento). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ButtonGroup|Efectos")
	TObjectPtr<USoundBase> ActivatedSound;

	/** Efecto visual al activarse el grupo, en el gestor (solo quien está en ese momento). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ButtonGroup|Efectos")
	TObjectPtr<UNiagaraSystem> ActivatedVFX;

	/** Sonido al desactivarse (solo con bOneShot = false). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ButtonGroup|Efectos")
	TObjectPtr<USoundBase> DeactivatedSound;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** ¿Está activado el grupo? Replicado en todas las máquinas. */
	UFUNCTION(BlueprintPure, Category = "ButtonGroup")
	bool IsGroupActive() const { return bGroupActive; }

	/**
	 * Registra un botón con este gestor en runtime (llamado por ATN_ButtonInteractable
	 * cuando tiene ManagerTag configurado). Idempotente: ignorar duplicados.
	 * Solo tiene efecto en el servidor.
	 */
	void RegisterButton(ATN_ButtonInteractable* Button);

private:
	/** Fuente de verdad del estado del grupo: la pone el servidor y llega a todos (también a quien entra tarde). */
	UPROPERTY(ReplicatedUsing = OnRep_GroupActive)
	bool bGroupActive = false;

	/** Servidor: ya se disparó un grupo bOneShot (no se vuelve a mirar). */
	bool bTriggered = false;

	/** Cliente: si ha movido ya los objetivos que no replican su movimiento (para no moverlos dos veces). */
	bool bClientActionsApplied = false;

	UFUNCTION()
	void OnRep_GroupActive();

	void OnButtonActivationChanged(ATN_ButtonInteractable* Button, bool bActivated);
	void CheckAndTrigger();
	void ApplyTriggerActions(bool bForward);

	/** Scan diferido (un tick después de BeginPlay) para registrar botones por tag. */
	void DeferredTagButtonScan();

	int32 GetEffectiveThreshold() const;

	/** Sonido y efecto del momento en cada máquina (el estado ya va en bGroupActive). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayGroupEffect(bool bActivated);

	void PlayGroupEffect(bool bActivated) const;
};
