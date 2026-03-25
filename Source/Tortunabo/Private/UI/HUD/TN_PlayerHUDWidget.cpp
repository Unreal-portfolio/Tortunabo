#include "UI/HUD/TN_PlayerHUDWidget.h"
#include "Player/TN_StaminaComponent.h"
#include "Player/TN_InventoryComponent.h"
#include "Player/TortugaCharacter.h"
#include "Core/TN_InventoryTypes.h"
#include "Core/TN_CoopPlayerState.h"
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
			const bool bHadStamina   = CachedStamina.IsValid();
			const bool bHadInventory = CachedInventory.IsValid();

			CachedStamina   = Pawn->FindComponentByClass<UTN_StaminaComponent>();
			CachedInventory = Pawn->FindComponentByClass<UTN_InventoryComponent>();

			// Forzar refresh completo cuando los componentes se re-encuentran
			// (nuevo pawn tras seamless travel). Sin esto, si la stamina tiene
			// el mismo valor que el último frame, bStaminaChanged=false y los
			// widgets permanecen ocultos indefinidamente.
			if ((!bHadStamina && CachedStamina.IsValid()) || (!bHadInventory && CachedInventory.IsValid()))
			{
				LastStamina       = -1.f;
				LastWeightPenalty = -1.f;
				bLastExhausted    = false;
				LastEquippedId    = NAME_None;
				LastStoredId      = NAME_None;
				UE_LOG(LogTemp, Log, TEXT("[PlayerHUD] Componentes re-encontrados tras travel — forzando refresh."));
			}
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
		const float Current     = CachedStamina->GetCurrentStamina();
		const float WeightPen   = CachedStamina->GetWeightPenalty();
		const bool  bExhausted  = CachedStamina->IsExhausted();

		const bool bStaminaChanged = !FMath::IsNearlyEqual(Current, LastStamina, 0.5f) || bExhausted != bLastExhausted;
		const bool bWeightChanged  = !FMath::IsNearlyEqual(WeightPen, LastWeightPenalty, 0.5f);

		if (bStaminaChanged || bWeightChanged)
		{
			LastStamina       = Current;
			bLastExhausted    = bExhausted;
			LastWeightPenalty = WeightPen;
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

	// ── DBNO / Revive ────────────────────────────────────────────────────────
	if (const APawn* Pawn = GetOwningPlayerPawn())
	{
		// Check DBNO via PlayerState
		if (const APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			if (const ATN_CoopPlayerState* PS = PC->GetPlayerState<ATN_CoopPlayerState>())
			{
				const bool bNowDBNO = PS->bIsDBNO;
				if (bNowDBNO != bLastDBNO)
				{
					bLastDBNO = bNowDBNO;
					OnDBNOStateChanged(bNowDBNO, PS->DBNOBleedoutTimeRemaining);
				}
				else if (bNowDBNO)
				{
					// Update bleedout remaining even if state didn't change
					OnDBNOStateChanged(true, PS->DBNOBleedoutTimeRemaining);
				}
			}
		}

		// Check revive channel via TortugaCharacter
		if (const ATortugaCharacter* TChar = Cast<ATortugaCharacter>(Pawn))
		{
			const bool bNowReviving = TChar->bIsReviving;
			const float NowProgress = TChar->ReviveProgress;
			if (bNowReviving != bLastReviving || !FMath::IsNearlyEqual(NowProgress, LastReviveProgress, 0.01f))
			{
				bLastReviving = bNowReviving;
				LastReviveProgress = NowProgress;
				OnReviveProgressUpdated(NowProgress, bNowReviving);
			}
		}
	}
}

// ── Stamina ───────────────────────────────────────────────────────────────────

void UTN_PlayerHUDWidget::RefreshStaminaWidgets()
{
	if (!CachedStamina.IsValid())
	{
		if (StaminaBar)      { StaminaBar->SetVisibility(ESlateVisibility::Hidden); }
		if (WeightPenaltyBar){ WeightPenaltyBar->SetVisibility(ESlateVisibility::Hidden); }
		if (ExhaustedRoot)   { ExhaustedRoot->SetVisibility(ESlateVisibility::Hidden); }
		if (StaminaText)     { StaminaText->SetVisibility(ESlateVisibility::Hidden); }
		return;
	}

	const float Current    = CachedStamina->GetCurrentStamina();
	const float MaxStam    = CachedStamina->GetMaxStamina();
	const float EffMax     = CachedStamina->GetEffectiveMaxStamina();
	const float WeightPen  = CachedStamina->GetWeightPenalty();
	const bool  bExhaust   = CachedStamina->IsExhausted();

	// Ratio de stamina actual vs el máximo BASE (para que la barra de stamina
	// y la de peso compartan la misma escala visual).
	const float StaminaRatio = (MaxStam > 0.f) ? FMath::Clamp(Current / MaxStam, 0.f, 1.f) : 0.f;
	// Ratio que ocupa la penalización de peso (zona oscura a la derecha).
	const float WeightRatio  = (MaxStam > 0.f) ? FMath::Clamp(WeightPen / MaxStam, 0.f, 1.f) : 0.f;

	if (StaminaBar)
	{
		StaminaBar->SetVisibility(ESlateVisibility::HitTestInvisible);
		StaminaBar->SetPercent(StaminaRatio);
	}

	// WeightPenaltyBar: superponer sobre StaminaBar, alineada a la derecha.
	// En el Widget Designer: mismo tamaño que StaminaBar, mismo anchor,
	// Fill Direction = Right to Left, color distinto (ej. marrón oscuro #5C3317).
	if (WeightPenaltyBar)
	{
		WeightPenaltyBar->SetVisibility(ESlateVisibility::HitTestInvisible);
		WeightPenaltyBar->SetPercent(WeightRatio);
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
		// Muestra stamina actual vs el efectivo (más informativo que el máximo base)
		StaminaText->SetText(FText::FromString(
			FString::Printf(TEXT("%.0f / %.0f"), Current, EffMax)));
	}

	OnStaminaUpdated(Current, MaxStam, bExhaust);
	OnWeightUpdated(WeightPen, MaxStam, EffMax);
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
