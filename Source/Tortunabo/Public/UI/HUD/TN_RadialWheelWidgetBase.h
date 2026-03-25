#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/HUD/TN_RadialWheelTypes.h"
#include "TN_RadialWheelWidgetBase.generated.h"

class UCanvasPanel;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTN_OnRadialSelectionChanged, int32, SelectedIndex, uint8, EntryId, FText, Label);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTN_OnRadialSelectionCleared);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTN_OnRadialSelectionConfirmed, int32, SelectedIndex, uint8, EntryId, FText, Label);

UCLASS(Blueprintable)
class TORTUNABO_API UTN_RadialWheelWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Radial")
	void SetEntries(const TArray<FTN_RadialWheelEntryView>& InEntries);

	UFUNCTION(BlueprintCallable, Category = "Radial")
	void UpdateInputVector(FVector2D InVector);

	UFUNCTION(BlueprintCallable, Category = "Radial")
	void ClearSelection();

	UFUNCTION(BlueprintCallable, Category = "Radial")
	bool TryConfirmSelection(FTN_RadialWheelEntryView& OutEntry);

	UFUNCTION(BlueprintPure, Category = "Radial")
	int32 GetSelectedIndex() const { return SelectedIndex; }

	UFUNCTION(BlueprintPure, Category = "Radial")
	const TArray<FTN_RadialWheelEntryView>& GetEntries() const { return Entries; }

	UFUNCTION(BlueprintPure, Category = "Radial")
	bool HasValidSelection() const;

	UPROPERTY(BlueprintAssignable, Category = "Radial")
	FTN_OnRadialSelectionChanged OnSelectionChanged;

	UPROPERTY(BlueprintAssignable, Category = "Radial")
	FTN_OnRadialSelectionCleared OnSelectionCleared;

	UPROPERTY(BlueprintAssignable, Category = "Radial")
	FTN_OnRadialSelectionConfirmed OnSelectionConfirmed;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Radial")
	TArray<FTN_RadialWheelEntryView> Entries;

	UPROPERTY(BlueprintReadOnly, Category = "Radial")
	int32 SelectedIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Radial")
	FVector2D CurrentInputVector = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radial", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float SelectionDeadzone = 0.35f;

	/** 90 = slice 0 arriba. 0 = slice 0 a la derecha. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radial")
	float SelectionAngleOffsetDegrees = 90.f;

	/** Radio en píxeles al que se colocan los labels/iconos dentro de RadialSlotsContainer. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Radial")
	float SlotRadius = 150.f;

	/** Tamaño del icono de cada slot (píxeles). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Radial")
	FVector2D SlotIconSize = FVector2D(48.f, 48.f);

	/** Punto central dentro de RadialSlotsContainer desde el que se distribuyen los slots.
	 *  Ajustar a la mitad del tamaño del CanvasPanel en el Blueprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Radial")
	FVector2D RadialCenter = FVector2D(0.f, 0.f);

	/**
	 * CanvasPanel en el que se generan automáticamente los widgets de cada slot.
	 * Añadir en el Designer del Blueprint hijo con el nombre exacto "RadialSlotsContainer".
	 * Sin nodos en el Event Graph: la implementación C++ de BP_OnEntriesSet lo rellena.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> RadialSlotsContainer;

	UFUNCTION(BlueprintNativeEvent, Category = "Radial")
	void BP_OnEntriesSet(const TArray<FTN_RadialWheelEntryView>& InEntries);

	UFUNCTION(BlueprintImplementableEvent, Category = "Radial")
	void BP_OnSelectionChanged(int32 InSelectedIndex, uint8 EntryId, const FText& Label);

	UFUNCTION(BlueprintImplementableEvent, Category = "Radial")
	void BP_OnSelectionCleared();

	UFUNCTION(BlueprintImplementableEvent, Category = "Radial")
	void BP_OnSelectionConfirmed(int32 InSelectedIndex, uint8 EntryId, const FText& Label);

private:
	void SetSelectedIndexInternal(int32 NewIndex);
};

