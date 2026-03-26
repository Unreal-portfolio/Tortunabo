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
 * El skin aplica un override de material a los componentes del cuerpo (Cuerpo, Pata1, etc.).
 * SkinId == NAME_None → sin skin (aspecto por defecto).
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

	/**
	 * Material que se aplica a los componentes "Body" y "Body1" del personaje y la estatua.
	 * Si es null, se restaura el material por defecto (sin skin).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	TObjectPtr<UMaterialInterface> BodyMaterial = nullptr;

	/**
	 * Material que se aplica a los componentes "Mesh1"–"Mesh5" y "Mesh13" del personaje y la estatua.
	 * Suele controlar el color de extremidades / detalles de piel.
	 * Si es null, se restaura el material por defecto de esos componentes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	TObjectPtr<UMaterialInterface> SkinMaterial = nullptr;

	/** Icono 2D para el selector de skins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	TObjectPtr<UTexture2D> Icon = nullptr;

	bool IsValid() const { return SkinId != NAME_None; }
};

