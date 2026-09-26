#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TN_ShellComponent.generated.h"

class ATortugaCharacter;
class USoundBase;

/**
 * @brief Componente que gobierna el estado de caparazón del personaje.
 *
 * Reglas principales:
 *  - Se entra y se sale con la misma tecla, solo desde el suelo, vivo, sin bucear
 *    y con las manos libres. Las condiciones viven en TN_ShellDecisions.h.
 *  - Encapsulado el personaje no se desplaza: el freno entra por el speed cap del
 *    UTN_StaminaComponent, nunca escribiendo MaxWalkSpeed a mano.
 *  - Dentro del caparazón se muere igual que fuera: no se toca ninguna guarda de
 *    daño ni de zona de muerte.
 *
 * Replicación: bIsInShell a todos (el resto de máquinas necesita el visual). El
 * estado es autoritativo del servidor; el cliente solo pide el cambio.
 *
 * Fase 1 del issue #6 — coger y lanzar a un compañero encapsulado llegan después,
 * sobre este estado.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TORTUNABO_API UTN_ShellComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTN_ShellComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * @brief Pide entrar o salir del caparazón. Server-authoritative.
	 * @note Llamable desde el cliente: reenvía a ServerToggleShell.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shell")
	void RequestToggleShell();

	UFUNCTION(BlueprintPure, Category = "Shell")
	bool IsInShell() const { return bIsInShell; }

	/**
	 * @brief Fuerza la salida del caparazón sin comprobar la permanencia mínima.
	 * @note Solo autoridad. Lo usan las rutas de muerte y derribo para que el
	 *       personaje no se quede con el speed cap pegado tras revivir.
	 */
	void ForceExitShell();

	/**
	 * @brief Mete al personaje en el caparazón sin las condiciones normales (suelo,
	 *        manos libres). Solo autoridad.
	 * @note Lo usan la caída desde altura (más de 5 m) y coger a una tortuga aturdida.
	 */
	void ForceEnterShell();

	/**
	 * @brief Bloquea o desbloquea la salida voluntaria del caparazón. Solo autoridad.
	 * @note Mientras la llevan o vuela tras un lanzamiento no puede salir: se
	 *       desbloquea al rebotar contra el suelo (UTN_CarryComponent).
	 */
	void SetExitLocked(bool bLocked) { bExitLocked = bLocked; }

	UFUNCTION(BlueprintPure, Category = "Shell")
	bool IsExitLocked() const { return bExitLocked; }

protected:
	/** Permanencia mínima (s) antes de poder salir. Evita el parpadeo al machacar la tecla. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shell", meta = (ClampMin = "0.0"))
	float MinTimeInShellSeconds = 0.3f;

	/** Sonido al meterse en el caparazón. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shell|Audio")
	TObjectPtr<USoundBase> EnterShellSound;

	/** Sonido al salir del caparazón. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shell|Audio")
	TObjectPtr<USoundBase> ExitShellSound;

private:
	/** @brief Server RPC: valida las condiciones y conmuta el estado. */
	UFUNCTION(Server, Reliable)
	void ServerToggleShell();

	UPROPERTY(ReplicatedUsing = OnRep_IsInShell)
	bool bIsInShell = false;

	/**
	 * Momento de servidor en que se entró al caparazón. Solo se usa con autoridad,
	 * así que no se replica.
	 */
	float ShellEnteredServerTime = 0.f;

	/** Salida voluntaria bloqueada (llevada / en vuelo tras un lanzamiento). Solo servidor. */
	bool bExitLocked = false;

	UFUNCTION()
	void OnRep_IsInShell();

	/**
	 * @brief Aplica los efectos del estado en la máquina local.
	 * @note Lo llaman OnRep_IsInShell y, en el servidor, SetShellState — en un
	 *       listen server OnRep no dispara en la máquina dueña de la variable,
	 *       mismo motivo por el que existe ApplyKnockdownVisual.
	 */
	void ApplyShellState(bool bInShell);

	/** @brief Escribe el estado con autoridad y lo aplica localmente. */
	void SetShellState(bool bInShell);

	/** @brief Devuelve el personaje dueño, o nullptr si el componente cuelga de otra cosa. */
	ATortugaCharacter* GetTurtleOwner() const;
};
