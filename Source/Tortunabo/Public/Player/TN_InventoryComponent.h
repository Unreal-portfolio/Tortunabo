#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/TN_InventoryTypes.h"
#include "TN_InventoryComponent.generated.h"

class UStaticMeshComponent;

/**
 * @brief Inventario de dos slots (equipado + guardado) replicado por jugador.
 *
 *  - Equipado: visible como mesh attacheado al socket EquippedAttachSocket; se consume con el input "Usar".
 *  - Guardado: invisible en el mundo; rota con el equipado mediante RotateItems.
 *  - El peso total afecta al StaminaComponent (reduce MaxStaminaEffective).
 *  - Server-authoritative: TryAddItem/RotateItems/Consume operan sólo si HasAuthority,
 *    aunque pueden llamarse vía wrappers desde clientes (Server RPC interno).
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TORTUNABO_API UTN_InventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTN_InventoryComponent();

	/** @brief Refresca el visual del ítem equipado tras possess (cliente y servidor). */
	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * @brief Intenta añadir un ítem al primer slot disponible (equipado > guardado).
	 * @return true si se añadió.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TryAddItem(const FTN_InventoryItem& NewItem);

	/**
	 * @brief Añade el ítem como equipado, opcionalmente sustituyendo el actual.
	 * @param bReplaceIfFull Si true y ya hay equipado, lo reemplaza tirando el viejo.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TryAddOrReplaceEquipped(const FTN_InventoryItem& NewItem, bool bReplaceIfFull = true);

	/** @brief Devuelve true si el ítem puede recibirse (slot libre o reemplazo permitido). */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool CanReceiveItem(const FTN_InventoryItem& NewItem, bool bAllowReplaceIfFull = true) const;

	/** @brief Rota equipado ↔ guardado. RPC al servidor desde cliente. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RotateItems();

	/** @brief Consume el ítem equipado (lo elimina del slot) y lo devuelve por referencia. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TryConsumeEquippedItem(FTN_InventoryItem& OutConsumedItem);

	/** @brief Extrae el ítem equipado sin consumirlo (lo saca del slot pero permite reusarlo). */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TryExtractEquippedItem(FTN_InventoryItem& OutExtractedItem);

	/**
	 * Busca en equipado y almacenado un ítem con UseType == InUseType.
	 * Si lo encuentra, lo consume (igual que TryConsumeEquippedItem) y lo devuelve.
	 * Útil para auto-consumo en eventos externos (ej: Tótem al morir).
	 * Solo funciona en autoridad.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TryConsumeItemByUseType(ETN_ItemUseType InUseType, FTN_InventoryItem& OutConsumedItem);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasEquippedItem() const { return bHasEquippedItem; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasStoredItem() const { return bHasStoredItem; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	FTN_InventoryItem GetEquippedItem() const { return EquippedItem; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	FTN_InventoryItem GetStoredItem() const { return StoredItem; }

	/** @brief Devuelve la suma de ItemWeight de todos los ítems llevados (equipado + guardado). */
	UFUNCTION(BlueprintPure, Category = "Inventory|Weight")
	float GetTotalCarriedWeight() const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Visual")
	FName EquippedAttachSocket = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Visual")
	FVector EquippedRelativeLocation = FVector(20.f, 0.f, 40.f);

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Visual")
	FRotator EquippedRelativeRotation = FRotator::ZeroRotator;

private:
	/** @brief Server RPC para que un cliente solicite rotar slots. */
	UFUNCTION(Server, Reliable)
	void ServerRotateItems();

	UPROPERTY(ReplicatedUsing = OnRep_EquippedItem)
	FTN_InventoryItem EquippedItem;

	UPROPERTY(Replicated)
	bool bHasEquippedItem = false;

	UPROPERTY(ReplicatedUsing = OnRep_StoredItem)
	FTN_InventoryItem StoredItem;

	UPROPERTY(Replicated)
	bool bHasStoredItem = false;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> EquippedVisualMesh;

	/** Componente padre del visual equipado. Guardado para compensar escala en RefreshEquippedVisual. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> VisualMeshParent;

	/** @brief OnRep: refresca el mesh visual del slot equipado tras una replicación. */
	UFUNCTION()
	void OnRep_EquippedItem();

	/** @brief OnRep: hook para HUD al cambiar el guardado (no tiene visual en mundo). */
	UFUNCTION()
	void OnRep_StoredItem();

	/** @brief Sincroniza el mesh attacheado con el EquippedItem actual. */
	void RefreshEquippedVisual();

	/** @brief Añade un ítem en el primer slot libre (server-side internal). */
	bool AddItemInternal(const FTN_InventoryItem& NewItem);

	/** @brief Reemplaza el equipado si bReplaceIfFull, si no usa el guardado (server-side internal). */
	bool AddOrReplaceEquippedInternal(const FTN_InventoryItem& NewItem, bool bReplaceIfFull);

	/** @brief Consume y devuelve el equipado (server-side internal). */
	bool ConsumeEquippedInternal(FTN_InventoryItem& OutItem);

	/** @brief Intercambia equipado ↔ guardado (server-side internal). */
	void SwapSlotsInternal();
};
