#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Materials/MaterialInterface.h"
#include "TN_CosmeticsTypes.generated.h"

class UStaticMesh;
class UTexture2D;

/**
 * Fila de DataTable que describe un casco cosmético.
 * Crear DT_Helmets en Content/Blueprints/Gameplay/Cosmetics/ con esta struct.
 * La RowName debe coincidir con el HelmetId usado en MP_GameInstance y PlayerState.
 *
 * Nombre del socket/componente en el personaje: "Sombrero"
 */
USTRUCT(BlueprintType)
struct FTN_HelmetData : public FTableRowBase
{
	GENERATED_BODY()

	/**
	 * Identificador único del casco.
	 * Debe coincidir con la RowName en DT_Helmets y con los IDs en DefaultUnlockedHelmets.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	FName HelmetId = NAME_None;

	/** Nombre legible mostrado en el menú de cosméticos. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	FText DisplayName;

	/**
	 * Mesh 3D que se instancia en el socket "Sombrero" del personaje al equiparlo.
	 * Si es null, se muestra el nombre pero sin mesh (útil para placeholders).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	TObjectPtr<UStaticMesh> DisplayMesh = nullptr;

	/**
	 * Icono 2D para el grid del menú de cosméticos.
	 * Si es null, el widget BP puede usar un placeholder.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/**
	 * Escala del mesh en el socket Sombrero.
	 * (1,1,1) = escala original. Ajusta para que encaje en la cabeza de la tortuga.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	FVector MeshScale = FVector::OneVector;

	/**
	 * Offset de posición relativo al socket Sombrero (cm).
	 * Ajusta para centrar el casco sobre la cabeza correctamente.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	FVector MeshOffset = FVector::ZeroVector;

	/**
	 * Offset de rotación relativo al socket Sombrero (grados).
	 * Ajusta si el casco importado tiene orientación diferente.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	FRotator MeshRotation = FRotator::ZeroRotator;

	bool IsValid() const { return HelmetId != NAME_None; }
};

/**
 * Fila de DataTable que describe un skin de personaje.
 * Crear DT_Skins en Content/Blueprints/Gameplay/Cosmetics/ con esta struct.
 * La RowName debe coincidir con el SkinId usado en MP_GameInstance y PlayerState.
 *
 * Mapeo de slots del SkM unificado (5 slots):
 *   Slot 0 → Barriga                         (BellyMaterial)
 *   Slot 1 → Brillo de los ojos              (EyeShineMaterial)
 *   Slot 2 → Ojos y boca                     (EyesMouthMaterial)
 *   Slot 3 → Cabeza, patas, brazos, cola     (SkinMaterial)
 *   Slot 4 → Caparazón principal             (ShellMaterial)
 *
 * Cada material es opcional: si está null se mantiene el material por defecto
 * de ese slot (cacheado en BeginPlay). SkinId == NAME_None → sin skin.
 */
USTRUCT(BlueprintType)
struct FTN_SkinData : public FTableRowBase
{
	GENERATED_BODY()

	/** Identificador único del skin. Debe coincidir con la RowName en DT_Skins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	FName SkinId = NAME_None;

	/** Nombre legible mostrado en el menú de cosméticos. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	FText DisplayName;

	/** Slot 0 — Barriga. Si null, se mantiene el material por defecto del SkM. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics|Slots")
	TObjectPtr<UMaterialInterface> BellyMaterial = nullptr;

	/** Slot 1 — Brillo de los ojos. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics|Slots")
	TObjectPtr<UMaterialInterface> EyeShineMaterial = nullptr;

	/** Slot 2 — Ojos y boca. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics|Slots")
	TObjectPtr<UMaterialInterface> EyesMouthMaterial = nullptr;

	/** Slot 3 — Cabeza, patas, brazos, cola. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics|Slots")
	TObjectPtr<UMaterialInterface> SkinMaterial = nullptr;

	/** Slot 4 — Caparazón principal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics|Slots")
	TObjectPtr<UMaterialInterface> ShellMaterial = nullptr;

	/** Icono 2D para el selector de skins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	TObjectPtr<UTexture2D> Icon = nullptr;

	bool IsValid() const { return SkinId != NAME_None; }
};

