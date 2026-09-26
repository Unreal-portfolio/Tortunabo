#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "TN_ShellComponent.generated.h"

class ATN_ShellBody;
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
 * Física propia: metida en el caparazón y suelta (nadie la lleva), la tortuga pasa a
 * ser un ATN_ShellBody, una caja con física que rueda, resbala y rebota. Body se
 * replica y cada máquina engancha la tortuga a su caja (ApplyBodyLocalState): el
 * movimiento del personaje se apaga, la cápsula solo solapa y cápsula y malla siguen
 * a la caja. Al salir, la tortuga se pone de pie donde quedó el caparazón.
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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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
	 * @param bPhysics    Con cuerpo físico (false al cogerla: la lleva otra tortuga).
	 * @param bExitOnRest Sale sola del caparazón cuando la caja se para (caída larga).
	 * @note Lo usan la caída desde altura (más de 5 m) y coger a una tortuga.
	 */
	void ForceEnterShell(bool bPhysics = true, bool bExitOnRest = false);

	/**
	 * @brief Servidor: suelta el caparazón como cuerpo físico con esa velocidad.
	 * @param bLaunched   Nace tumbado en la posición del actor y, si va rápido, dando
	 *                    volteretas (lanzamiento, soltar). Si no, nace de pie en el
	 *                    tronco y se vuelca hacia delante sobre la tripa (entrar a mano).
	 * @param bExitOnRest Sale del caparazón cuando la caja se queda quieta.
	 */
	void StartBody(const FVector& Velocity, bool bLaunched, bool bExitOnRest);

	/** @brief Servidor: pone a la tortuga de pie donde está el caparazón y destruye la caja. */
	void StopBody();

	/** Caja física actual (replicada; nullptr si no hay). */
	ATN_ShellBody* GetBody() const { return Body; }

	/** true si en esta máquina la tortuga está enganchada a su caja (la sigue y no se mueve sola). */
	bool HasLocalBody() const { return bBodyLocalApplied; }

	/** Todas las máquinas: engancha la tortuga a esta caja (la llama la propia caja al llegar a un cliente). */
	void AdoptBody(ATN_ShellBody* InBody);

	/** Todas las máquinas: coloca cápsula y malla sobre la caja (Tick de la caja, tras la física). */
	void FollowBody(ATN_ShellBody* InBody);

	/** Local: suelta la caja sin esperar a la réplica (al engancharla a quien la coge). */
	void DropLocalBody();

	/** Servidor: la caja se ha parado tras un lanzamiento o una caída → sale del caparazón. */
	void NotifyBodyAtRest();

	/** Servidor: la caja ha caído al agua → sale del caparazón y nada. */
	void NotifyBodyInWater();

	/** La caja se ha destruido sin pasar por StopBody (caída fuera del mundo, por ejemplo). */
	void NotifyBodyEnded(ATN_ShellBody* InBody);

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

	/**
	 * @brief Escribe el estado con autoridad y lo aplica localmente. Al entrar, suelta
	 *        el cuerpo físico si bPhysics y nadie la lleva; al salir, lo quita.
	 */
	void SetShellState(bool bInShell, bool bPhysics = true, bool bExitOnRest = false);

	/** Caja física del caparazón (solo mientras está dentro y suelta). */
	UPROPERTY(ReplicatedUsing = OnRep_Body)
	TObjectPtr<ATN_ShellBody> Body;

	UFUNCTION()
	void OnRep_Body();

	/**
	 * @brief Engancha (o suelta) la tortuga a su caja en esta máquina: movimiento del
	 *        personaje apagado, cápsula que solo solapa y sin réplica de movimiento del
	 *        personaje (cada máquina la sigue a partir de la caja replicada).
	 */
	void ApplyBodyLocalState(bool bOn);

	/** Suelta la caja enganchada en esta máquina: coloca a la tortuga de pie y restaura. */
	void ReleaseLocalBody();

	/** Coloca la tortuga de pie donde está la caja (suelo debajo; en el agua, en su centro). */
	void PlaceStandingFromBox(const FTransform& BoxWorld, bool bInWater, const AActor* IgnoreActor);

	/** Caja enganchada en esta máquina. */
	TWeakObjectPtr<ATN_ShellBody> LocalBody;
	bool bBodyLocalApplied = false;

	/** Última transformación seguida de la caja (para ponerse de pie si la caja ya no está). */
	FTransform LastBoxTransform;
	bool bHasLastBox = false;

	/** Colisión de la cápsula y suavizado de red antes de engancharse, para restaurarlos. */
	FName SavedCapsuleProfile = NAME_None;
	TEnumAsByte<ECollisionEnabled::Type> SavedCapsuleEnabled = ECollisionEnabled::QueryAndPhysics;
	FCollisionResponseContainer SavedCapsuleResponses;
	uint8 SavedSmoothingMode = 0;

	/** @brief Devuelve el personaje dueño, o nullptr si el componente cuelga de otra cosa. */
	ATortugaCharacter* GetTurtleOwner() const;
};
