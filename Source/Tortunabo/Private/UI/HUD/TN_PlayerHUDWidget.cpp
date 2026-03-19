#include "UI/HUD/TN_PlayerHUDWidget.h"
#include "Player/TN_StaminaComponent.h"
#include "Player/TN_InventoryComponent.h"
#include "Core/TN_InventoryTypes.h"
#include "Components/ProgressBar.h"
#include "Components/Widget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Styling/SlateBrush.h"
#include "GameFramework/Pawn.h"

void UTN_PlayerHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (const APawn* Pawn = GetOwningPlayerPawn())
	{
		CachedStamina    = Pawn->FindComponentByClass<UTN_StaminaComponent>();
		CachedInventory  = Pawn->FindComponentByClass<UTN_InventoryComponent>();
	}

	// El selector siempre está sobre el slot equipado — mostrarlo desde el inicio.
	if (SlotEquippedSelector)
	{
		SlotEquippedSelector->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	RefreshStaminaWidgets();
	RefreshInventoryWidgets();
}

void UTN_PlayerHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Re-cachear si el pawn cambió (posesión, travel, etc.)
	if (!CachedStamina.IsValid() || !CachedInventory.IsValid())
	{
		if (const APawn* Pawn = GetOwningPlayerPawn())
		{
			CachedStamina   = Pawn->FindComponentByClass<UTN_StaminaComponent>();
			CachedInventory = Pawn->FindComponentByClass<UTN_InventoryComponent>();
		}
	}

	// Throttle compartido: refrescar a ~20 fps
	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator < kRefreshInterval)
	{
		return;
	}
	RefreshAccumulator = 0.f;

	// ── Stamina ──────────────────────────────────────────────────────────────
	if (CachedStamina.IsValid())
	{
		const float Current    = CachedStamina->GetCurrentStamina();
		const bool  bExhausted = CachedStamina->IsExhausted();

		if (!FMath::IsNearlyEqual(Current, LastStamina, 0.5f) || bExhausted != bLastExhausted)
		{
			LastStamina    = Current;
			bLastExhausted = bExhausted;
			RefreshStaminaWidgets();
		}
	}

	// ── Inventario ────────────────────────────────────────────────────────────
	if (CachedInventory.IsValid())
	{
		const FName EquippedId = CachedInventory->HasEquippedItem()
			? CachedInventory->GetEquippedItem().ItemId : NAME_None;
		const FName StoredId   = CachedInventory->HasStoredItem()
			? CachedInventory->GetStoredItem().ItemId   : NAME_None;

		if (EquippedId != LastEquippedId || StoredId != LastStoredId)
		{
			LastEquippedId = EquippedId;
			LastStoredId   = StoredId;
			RefreshInventoryWidgets();
		}
	}
}

// ── Stamina ───────────────────────────────────────────────────────────────────

void UTN_PlayerHUDWidget::RefreshStaminaWidgets()
{
	if (!CachedStamina.IsValid())
	{
		if (StaminaBar)    { StaminaBar->SetVisibility(ESlateVisibility::Hidden); }
		if (ExhaustedRoot) { ExhaustedRoot->SetVisibility(ESlateVisibility::Hidden); }
		if (StaminaText)   { StaminaText->SetVisibility(ESlateVisibility::Hidden); }
		return;
	}

	const float Current  = CachedStamina->GetCurrentStamina();
	const float MaxStam  = CachedStamina->GetMaxStamina();
	const bool  bExhaust = CachedStamina->IsExhausted();
	const float Ratio    = (MaxStam > 0.f) ? FMath::Clamp(Current / MaxStam, 0.f, 1.f) : 0.f;

	if (StaminaBar)
	{
		StaminaBar->SetVisibility(ESlateVisibility::HitTestInvisible);
		StaminaBar->SetPercent(Ratio);
	}

	if (ExhaustedRoot)
	{
		ExhaustedRoot->SetVisibility(bExhaust
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Hidden);
	}

	if (StaminaText)
	{
		StaminaText->SetVisibility(ESlateVisibility::HitTestInvisible);
		StaminaText->SetText(FText::FromString(
			FString::Printf(TEXT("%.0f / %.0f"), Current, MaxStam)));
	}

	OnStaminaUpdated(Current, MaxStam, bExhaust);
}

// ── Inventory ─────────────────────────────────────────────────────────────────

void UTN_PlayerHUDWidget::RefreshInventoryWidgets()
{
	const bool bHasEquipped = CachedInventory.IsValid() && CachedInventory->HasEquippedItem();
	const bool bHasStored   = CachedInventory.IsValid() && CachedInventory->HasStoredItem();

	UTexture2D* EquippedIcon = bHasEquipped ? CachedInventory->GetEquippedItem().ItemIcon.Get() : nullptr;
	UTexture2D* StoredIcon   = bHasStored   ? CachedInventory->GetStoredItem().ItemIcon.Get()   : nullptr;

	// Slot equipado
	if (SlotEquippedImage)
	{
		if (EquippedIcon)
		{
			SlotEquippedImage->SetBrushFromTexture(EquippedIcon, /*bMatchSize=*/false);
			SlotEquippedImage->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			// Sin ítem: mostrar el slot vacío (imagen transparente)
			SlotEquippedImage->SetColorAndOpacity(FLinearColor::Transparent);
		}
	}

	// Slot guardado
	if (SlotStoredImage)
	{
		if (StoredIcon)
		{
			SlotStoredImage->SetBrushFromTexture(StoredIcon, /*bMatchSize=*/false);
			SlotStoredImage->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			SlotStoredImage->SetColorAndOpacity(FLinearColor::Transparent);
		}
	}

	// Selector: siempre sobre el slot equipado
	if (SlotEquippedSelector)
	{
		SlotEquippedSelector->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	OnInventoryUpdated(EquippedIcon, bHasEquipped, StoredIcon, bHasStored);
}
