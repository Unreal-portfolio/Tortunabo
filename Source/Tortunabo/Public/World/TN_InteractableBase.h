#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TN_InteractableBase.generated.h"

class UStaticMeshComponent;
class UWidgetComponent;
class UTN_InteractPromptWidget;
class UUserWidget;

/**
 * @brief Clase base abstracta para todos los actores interactuables (pickup, estaciones, botones, etc.).
 *
 * Provee el contrato común: CanInteract/Interact, prompt 3D con widget, distancia máxima,
 * helpers para ocultar/mostrar el mesh visual sin desactivar el overlap.
 * Las subclases (TN_PickupInteractableBase, TN_DirectInteractableBase, etc.) implementan el comportamiento.
 */
UCLASS(Abstract)
class TORTUNABO_API ATN_InteractableBase : public AActor
{
	GENERATED_BODY()

public:
	ATN_InteractableBase();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	virtual bool CanInteract(APawn* Interactor) const;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	virtual void Interact(APawn* Interactor);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	float GetInteractionDistance() const { return InteractionDistance; }

	/**
	 * Punto desde el que se mide la distancia de interacción (el escaneo del personaje y la validación del servidor
	 * usan el mismo). Por defecto, el origen del actor; los actores grandes (puesto de la tienda, probador) lo ponen
	 * delante de su mostrador o de su puerta.
	 */
	virtual FVector GetInteractionPoint() const { return GetActorLocation(); }

	/**
	 * Punto de interacción visto desde Interactor (el escaneo del personaje y la validación del servidor usan este).
	 * Por defecto, GetInteractionPoint(); los decorados grandes que se rebuscan (ATN_ProcSearchSpot) dan el punto de
	 * su borde más cercano a Interactor, para que se pueda rebuscar desde cualquier lado.
	 */
	virtual FVector GetInteractionPointFor(const APawn* Interactor) const { return GetInteractionPoint(); }

	// ── Interacción de mantener la tecla (p. ej. rebuscar un decorado) ──────
	// El cliente avisa al empezar y al soltar (ATortugaCharacter::ServerBeginHoldInteract / ServerEndHoldInteract);
	// el tiempo lo cuenta el servidor, que es quien decide cuándo se completa.

	/** Segundos que hay que mantener la tecla; 0 = interacción de pulsar (Interact). */
	virtual float GetHoldDuration() const { return 0.f; }

	/** Servidor: Interactor empieza a mantener la tecla (ya está validado que está cerca y que puede). */
	virtual void BeginHoldInteract(APawn* Interactor) {}

	/** Servidor: Interactor suelta la tecla antes de acabar (cancela lo que llevara). */
	virtual void EndHoldInteract(APawn* Interactor) {}

	/** Progreso [0..1] de lo que mantiene Interactor según el estado replicado; negativo si no mantiene nada aquí. */
	virtual float GetHoldProgress(const APawn* Interactor) const { return -1.f; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	FText GetPromptText() const { return PromptText; }

	/** Oculta el mesh estático del interactuable; la hitbox de overlap permanece activa. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void HideInteractableMesh();

	/** Muestra el mesh estático del interactuable (reverso de HideInteractableMesh). */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void ShowInteractableMesh();


protected:
	/**
	 * Componente raíz invisible. Al ser la raíz, su posición define la
	 * posición del actor en el mundo. El Mesh es un hijo, por lo que
	 * Mesh->SetRelativeLocation() solo mueve el visual, NO el actor.
	 * Sin este componente intermedio, SetRelativeLocation en el Mesh (que era
	 * la raíz) movía el actor completo al origen del mundo.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<USceneComponent> SceneRoot;

	/**
	 * Mesh del interactuable. Hijo de SceneRoot.
	 * Es el componente root del actor.
	 * Ser root (UStaticMeshComponent = UPrimitiveComponent) permite que
	 * UWorld::FindTeleportSpot calcule bounds correctamente al spawnear el actor.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction|UI")
	TObjectPtr<UWidgetComponent> PromptWidgetComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction")
	float InteractionDistance = 250.f;

	/**
	 * Desplazamiento en Z del mesh respecto al pivot del actor (cm).
	 * Úsalo para que el mesh no clipe con el suelo cuando su pivote está en el centro.
	 * Para pickups dinámicos, InitializeFromInventoryItem calcula este valor
	 * automáticamente desde los bounds del mesh.
	 * Para actores colocados en el nivel: ajusta por instancia o usa snap-to-floor (End).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float MeshFloorOffset = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|UI")
	FText PromptText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|UI")
	TSubclassOf<UUserWidget> PromptWidgetClass;

	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	void OnInteracted(APawn* Interactor);
	virtual void OnInteracted_Implementation(APawn* Interactor);

private:
	UPROPERTY(ReplicatedUsing = OnRep_InteractionEnabled)
	bool bInteractionEnabled = true;

	UFUNCTION()
	void OnRep_InteractionEnabled();

	void ApplyInteractionEnabledState();

	/**
	 * Estado replicado de visibilidad del mesh. HideInteractableMesh / ShowInteractableMesh
	 * lo modifican y disparan OnRep_MeshHidden en clientes. Sin esto, llamar
	 * Mesh->SetVisibility(false) solo tenía efecto en server (visibility no se replica).
	 */
	UPROPERTY(ReplicatedUsing = OnRep_MeshHidden)
	bool bMeshHidden = false;

	UFUNCTION()
	void OnRep_MeshHidden();

protected:
	void SetInteractionEnabled(bool bEnabled);
};
