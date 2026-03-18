#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TN_PlayerHUDWidget.generated.h"

class UProgressBar;
class UWidget;
class UTextBlock;
class UTN_StaminaComponent;

/**
 * HUD principal del jugador.
 * Contiene la barra de stamina y será el contenedor de toda la UI en pantalla del personaje.
 *
 * Widgets opcionales — nómbralos EXACTAMENTE igual en el BP Designer:
 *   - StaminaBar       (UProgressBar)  → relleno proporcional a stamina actual/max
 *   - ExhaustedRoot    (cualquier widget) → visible solo durante penalización por agotamiento
 *   - StaminaText      (UTextBlock)    → opcional, muestra "120 / 200"
 */
UCLASS()
class TORTUNABO_API UTN_PlayerHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ── Stamina bar ───────────────────────────────────────────────────────────

	/** Barra de stamina. En el BP Designer nómbrala exactamente "StaminaBar". */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> StaminaBar;

	/** Widget que se muestra cuando el jugador está penalizado por agotamiento (ej. un icono rojo). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ExhaustedRoot;

	/** TextBlock opcional que muestra "120 / 200". */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StaminaText;

	// ── Blueprint hooks ───────────────────────────────────────────────────────

	/**
	 * Llamado cada tick cuando la stamina cambia. Úsalo en BP para animar la barra,
	 * cambiar colores, etc.
	 * @param CurrentStamina  Stamina actual (0..MaxStamina)
	 * @param MaxStamina      Stamina máxima
	 * @param bExhausted      True mientras dura la penalización por agotamiento
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Stamina")
	void OnStaminaUpdated(float CurrentStamina, float MaxStamina, bool bExhausted);

private:
	void RefreshStaminaWidgets();

	TWeakObjectPtr<UTN_StaminaComponent> CachedStamina;

	float LastStamina   = -1.f;
	bool  bLastExhausted = false;

	static constexpr float kRefreshInterval = 0.05f; // 20 fps de refresco UI
	float RefreshAccumulator = 0.f;
};

