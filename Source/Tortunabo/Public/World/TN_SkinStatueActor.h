#pragma once

#include "CoreMinimal.h"
#include "World/TN_DirectInteractableBase.h"
#include "TN_SkinStatueActor.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * Tipo de cosmético que gestiona esta estatua.
 * Añade nuevas entradas aquí para soportar futuros accesorios.
 */
UENUM(BlueprintType)
enum class ETNCosmeticType : uint8
{
	Skin   UMETA(DisplayName = "Skin de Personaje"),
	Helmet UMETA(DisplayName = "Sombrero / Accesorio"),
};

/**
 * ATN_SkinStatueActor — Estatua de cosmético en el lobby.
 *
 * Una sola clase para todos los tipos de cosmético (skins, sombreros, futuros accesorios).
 * Coloca instancias Blueprint en LVL_HQ y configura:
 *   - CosmeticType : qué tipo de cosmético aplica
 *   - CosmeticId   : ID que debe coincidir con una fila del DataTable correspondiente
 *                    (NAME_None = estatua especial "quitar cosmético")
 *
 * Al interactuar:
 *   - Si el jugador ya lleva este cosmético → lo desequipa.
 *   - Si lleva otro o ninguno → equipa este.
 *
 * El cambio persiste en MP_GameInstance y sobrevive a los viajes entre mapas.
 *
 * Setup rápido en el editor:
 *   1. Crear BP basado en este actor (BP_Statue_<ID>).
 *   2. Asignar CosmeticType y CosmeticId.
 *   3. Asignar PreviewMesh con el turtle mostrando el cosmético (visual de la estatua).
 *   4. Colocar en LVL_HQ.
 */
UCLASS()
class TORTUNABO_API ATN_SkinStatueActor : public ATN_DirectInteractableBase
{
	GENERATED_BODY()

public:
	ATN_SkinStatueActor();

	virtual void BeginPlay() override;
	virtual void Interact(APawn* Interactor) override;

	/** Tipo de cosmético que gestiona esta estatua. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetic Statue")
	ETNCosmeticType CosmeticType = ETNCosmeticType::Skin;

	/**
	 * ID del cosmético que aplica esta estatua.
	 * Skin   → RowName en DT_Skins
	 * Helmet → RowName en DT_Helmets
	 * NAME_None → estatua "quitar cosmético" (restaura aspecto por defecto).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetic Statue")
	FName CosmeticId = NAME_None;

	/**
	 * Mesh preview del personaje mostrando el cosmético.
	 * Sirve de visual de la estatua en el lobby.
	 * Para skins: asigna el skeletal mesh de la tortuga aquí y se le aplicará el material.
	 * Para cascos: puede quedar vacío; el mesh del casco se coloca en HelmetPreviewComp.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetic Statue")
	TObjectPtr<USkeletalMeshComponent> PreviewMesh;

	/**
	 * Punto de anclaje del sombrero en la estatua.
	 * Colócalo sobre la cabeza del personaje en el Blueprint para que el casco
	 * aparezca en la posición correcta (igual que el SceneComponent "Sombrero" del personaje).
	 * HelmetPreviewComp se adjunta a este componente.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetic Statue")
	TObjectPtr<USceneComponent> SombreroSocket;

	/**
	 * Componente donde se muestra el mesh 3D del casco/accesorio en la estatua.
	 * Se posiciona automáticamente en BeginPlay usando los datos de DT_Helmets.
	 * Adjunto a SombreroSocket → posiciona SombreroSocket en el BP para ajustar el origen.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetic Statue")
	TObjectPtr<UStaticMeshComponent> HelmetPreviewComp;

private:
	void ApplyPreviewCosmetic();
};
