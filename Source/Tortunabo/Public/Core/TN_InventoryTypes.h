#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "TN_InventoryTypes.generated.h"

class UStaticMesh;
class UTexture2D;
class USoundBase;
class ATN_PickupInteractableBase;
class ATN_ThrowableItemActor;

UENUM(BlueprintType)
enum class ETN_ItemUseType : uint8
{
	None UMETA(DisplayName = "None"),
	SelfStaminaBoost UMETA(DisplayName = "Self Stamina Boost"),
	Throwable UMETA(DisplayName = "Throwable"),
	BigHead UMETA(DisplayName = "Big Head")
};

/**
 * Definición completa de un ítem del juego.
 * Hereda FTableRowBase → se puede usar directamente como fila de DataTable.
 * Para añadir un ítem nuevo: añadir fila en DT_Items, sin crear clases nuevas.
 */
USTRUCT(BlueprintType)
struct FTN_InventoryItem : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FName ItemId = NAME_None;

	/**
	 * Icono del ítem para la UI de inventario (HUD).
	 * Asigna una Texture2D en el DataTable o en el BP del pickup.
	 * Si es null, el slot del HUD aparecerá vacío.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|UI")
	TObjectPtr<UTexture2D> ItemIcon = nullptr;

	/** Mesh mostrada cuando el ítem está equipado en el jugador Y en el suelo como pickup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TObjectPtr<UStaticMesh> EquippedMesh = nullptr;

	/**
	 * Escala del mesh en espacio mundo.
	 * Se aplica al visual equipado en mano (compensando la escala del personaje para
	 * evitar deformaciones) y al mesh del pickup en el suelo.
	 * Ajusta este valor en DT_Items para cambiar el tamaño sin tocar el asset.
	 * Ejemplo para bola pequeña: (0.25, 0.25, 0.25)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FVector EquippedMeshScale = FVector(1.f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Use")
	ETN_ItemUseType UseType = ETN_ItemUseType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Use", meta = (ClampMin = "0.0"))
	float StaminaUnlimitedDurationSeconds = 4.0f;

	/**
	 * Segundos de penalización de exhaustión tras que el boost de stamina expire.
	 * 0 = sin penalización. Permite configurar penalizaciones distintas por ítem.
	 * Sobreescribe el valor global de TN_StaminaComponent::PostBoostExhaustionSeconds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Use", meta = (ClampMin = "0.0"))
	float PostBoostExhaustionSeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Use", meta = (ClampMin = "0.0"))
	float ThrowSpeed = 1800.0f;

	/**
	 * Duración de vida del proyectil (s). 0 = usar el default del BP del lanzable (MaxLifeSeconds).
	 * Permite que ítems distintos tengan proyectiles que duren más o menos.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Use", meta = (ClampMin = "0.0"))
	float ThrowableLifeSpanSeconds = 0.f;

	/**
	 * Peso del ítem en unidades arbitrarias.
	 * Cada unidad reduce la stamina máxima efectiva del portador según
	 * UTN_StaminaComponent::StaminaPerWeightUnit (default 20 stamina/unidad).
	 * Ejemplo: ItemWeight=2 con StaminaPerWeightUnit=20 → -40 stamina máx.
	 * 0 = sin penalización.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Weight", meta = (ClampMin = "0.0", ClampMax = "20.0"))
	float ItemWeight = 0.0f;

	/**
	 * Sonido que se reproduce al recoger el ítem. Nulo = sin sonido.
	 * Se reproduce localmente en el cliente que recoge (spatialized = false).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Audio")
	TObjectPtr<USoundBase> PickupSound = nullptr;

	/**
	 * Sonido que se reproduce al consumir/usar el ítem. Nulo = sin sonido.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Audio")
	TObjectPtr<USoundBase> UseSound = nullptr;

	/**
	 * Clase del actor pickup que aparece en el mundo.
	 * Para el sistema data-driven, apuntar siempre a BP_GenericPickup.
	 * Solo crear clase específica si el pickup tiene lógica BP diferente.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|World")
	TSubclassOf<ATN_PickupInteractableBase> PickupActorClass;

	/**
	 * Clase del proyectil que se lanza.
	 * Para el sistema data-driven, apuntar siempre a BP_GenericThrowable.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|World")
	TSubclassOf<ATN_ThrowableItemActor> ThrowableActorClass;

	bool IsValid() const
	{
		return ItemId != NAME_None;
	}
};

