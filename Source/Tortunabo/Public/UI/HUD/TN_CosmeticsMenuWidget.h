#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/TN_CosmeticsTypes.h"
#include "TN_CosmeticsMenuWidget.generated.h"

class UButton;
class UPanelWidget;
class UMP_GameInstance;
class AMP_GamePlayerController;

/**
 * Widget base del menú de cosméticos.
 *
 * ─── Widgets opcionales en el BP Designer (nombres exactos) ───────────────────
 *   UnequipButton  (UButton)      → al hacer click llama RequestUnequipHelmet()
 *   CloseButton    (UButton)      → al hacer click cierra el menú
 *   HelmetGrid     (UPanelWidget) → cualquier panel (WrapBox, UniformGridPanel…)
 *                                   para alojar las entradas del grid. El BP lo
 *                                   puebla respondiendo a OnHelmetGridRefreshed.
 * ─────────────────────────────────────────────────────────────────────────────
 *
 * Flujo de uso:
 *   1. ATN_CosmeticsStationInteractable::Interact → ClientOpenCosmeticsMenu
 *   2. MP_GamePlayerController::OpenCosmeticsMenu → CreateWidget<UTN_CosmeticsMenuWidget>
 *   3. NativeConstruct → RefreshHelmetGrid → OnHelmetGridRefreshed (BP)
 *   4. BP construye visualmente los slots llamando GetHelmetData(HelmetId) por cada uno.
 *   5. Slot click → RequestEquip(HelmetId) → ServerSetEquippedHelmet → OnRep → UpdateHelmetMesh
 *   6. CloseButton / input → CloseMenu → restaura GameOnly input mode
 */
UCLASS()
class TORTUNABO_API UTN_CosmeticsMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ── API pública ──────────────────────────────────────────────────────────

	/**
	 * Refresca el grid con los cascos desbloqueados del jugador local.
	 * Se llama automáticamente en NativeConstruct; también puedes llamarla
	 * desde BP si se desbloquea un casco mientras el menú está abierto.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	void RefreshHelmetGrid();

	/**
	 * Equipa el casco con el ID dado.
	 * Llamar con NAME_None es equivalente a RequestUnequip().
	 * Envía el RPC al servidor; todos los jugadores verán el cambio.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	void RequestEquip(FName HelmetId);

	/** Desequipa el casco actual. */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	void RequestUnequip();

	/**
	 * Cierra el menú y restaura el modo de input de gameplay (sin cursor).
	 * Llama a esta función desde el botón de cierre o desde input (Escape).
	 */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	void CloseMenu();

	/**
	 * Obtiene los datos de display de un casco por su ID.
	 * Úsalo en el BP para construir cada entrada del grid:
	 *   icono, nombre, mesh preview, etc.
	 * Devuelve false si el ID no existe en DT_Helmets.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cosmetics")
	bool GetHelmetData(FName HelmetId, FTN_HelmetData& OutData) const;

	/**
	 * ID del casco que lleva actualmente el jugador local.
	 * Útil para resaltar la entrada activa en el grid.
	 */
	UFUNCTION(BlueprintPure, Category = "Cosmetics")
	FName GetEquippedHelmetId() const;

	// ── BlueprintImplementableEvents ─────────────────────────────────────────

	/**
	 * Llamado cuando el grid debe ser (re)construido.
	 * En el evento BP: itera sobre UnlockedHelmetIds, crea un widget por cada uno
	 * usando GetHelmetData(Id) para obtener icono/nombre/etc.
	 * CurrentEquippedId sirve para resaltar visualmente el casco equipado.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cosmetics")
	void OnHelmetGridRefreshed(const TArray<FName>& UnlockedHelmetIds, FName CurrentEquippedId);

	/**
	 * Llamado tras equipar/desequipar un casco (confirmación local inmediata).
	 * Úsalo para actualizar el resaltado del grid sin esperar al OnRep.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cosmetics")
	void OnHelmetEquipped(FName NewHelmetId);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ── Widgets opcionales (BindWidgetOptional) ───────────────────────────────

	/** Botón de desequipar. Nómbralo exactamente "UnequipButton" en el BP Designer. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> UnequipButton;

	/** Botón de cierre. Nómbralo exactamente "CloseButton" en el BP Designer. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	/**
	 * Panel que contiene las entradas del grid (WrapBox, UniformGridPanel, etc.).
	 * Nómbralo exactamente "HelmetGrid" en el BP Designer.
	 * Normalmente se puebla desde el evento BP OnHelmetGridRefreshed.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> HelmetGrid;

private:
	UFUNCTION()
	void OnUnequipClicked();

	UFUNCTION()
	void OnCloseClicked();

	AMP_GamePlayerController* GetOwningPC() const;
	UMP_GameInstance* GetOwningGI() const;
};

