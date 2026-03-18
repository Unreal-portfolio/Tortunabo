#include "UI/HUD/TN_PlayerHUDWidget.h"
#include "Player/TN_StaminaComponent.h"
#include "Components/ProgressBar.h"
#include "Components/Widget.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"

void UTN_PlayerHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Intentar cachear el componente de stamina del pawn local
	if (const APawn* Pawn = GetOwningPlayerPawn())
	{
		CachedStamina = Pawn->FindComponentByClass<UTN_StaminaComponent>();
	}

	RefreshStaminaWidgets();
}

void UTN_PlayerHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Re-cachear si el pawn cambió (posesión, travel, etc.)
	if (!CachedStamina.IsValid())
	{
		if (const APawn* Pawn = GetOwningPlayerPawn())
		{
			CachedStamina = Pawn->FindComponentByClass<UTN_StaminaComponent>();
		}
	}

	if (!CachedStamina.IsValid())
	{
		return;
	}

	// Throttle: refrescar a ~20 fps para no sobrecargar la UI
	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator < kRefreshInterval)
	{
		return;
	}
	RefreshAccumulator = 0.f;

	const float Current    = CachedStamina->GetCurrentStamina();
	const bool  bExhausted = CachedStamina->IsExhausted();

	// Solo actualizar si algo cambió (evitar llamadas redundantes a widgets)
	if (FMath::IsNearlyEqual(Current, LastStamina, 0.5f) && bExhausted == bLastExhausted)
	{
		return;
	}

	LastStamina    = Current;
	bLastExhausted = bExhausted;

	RefreshStaminaWidgets();
}

void UTN_PlayerHUDWidget::RefreshStaminaWidgets()
{
	if (!CachedStamina.IsValid())
	{
		// Sin pawn todavía: ocultar todo
		if (StaminaBar)      { StaminaBar->SetVisibility(ESlateVisibility::Hidden); }
		if (ExhaustedRoot)   { ExhaustedRoot->SetVisibility(ESlateVisibility::Hidden); }
		if (StaminaText)     { StaminaText->SetVisibility(ESlateVisibility::Hidden); }
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
		ExhaustedRoot->SetVisibility(bExhaust ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	if (StaminaText)
	{
		StaminaText->SetVisibility(ESlateVisibility::HitTestInvisible);
		StaminaText->SetText(FText::FromString(
			FString::Printf(TEXT("%.0f / %.0f"), Current, MaxStam)
		));
	}

	OnStaminaUpdated(Current, MaxStam, bExhaust);
}


