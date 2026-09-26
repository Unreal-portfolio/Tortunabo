#pragma once

#include "CoreMinimal.h"
#include "UI/HUD/TN_PlayerHUDWidget.h"
#include "UI/HUD/TN_CoopFlowHUDWidget.h"
#include "TN_RunHUDWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UProgressBar;
class UTextBlock;

/**
 * @brief HUD de la tortuga en partida, construido en código con el estilo de TN_HUDStyle (sin Widget Blueprint).
 *
 * Hereda toda la lógica de UTN_PlayerHUDWidget (estamina y peso, inventario, puntos) creando él mismo los widgets
 * que esa clase enlaza por nombre, y añade lo que antes quedaba para el Blueprint:
 *  - Estamina: barra redondeada sin número que pasa de turquesa a ámbar y a coral al vaciarse, con la zona
 *    bloqueada por el peso encima y el aviso de agotada que late.
 *  - Inventario: hueco del objeto equipado (marco turquesa) y del guardado.
 *  - Puntos: insignia con estrella que salta al sumar.
 *  - Tormenta: aviso coral con la cuenta atrás mientras se está dentro.
 *  - Derribado y reanimación: aviso con el tiempo que queda y barra de la reanimación que se está haciendo.
 */
UCLASS()
class TORTUNABO_API UTN_RunHUDWidget : public UTN_PlayerHUDWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildTree();
	void TickStamina(float DeltaTime);
	void TickScore(float DeltaTime);
	void TickAlerts(float DeltaTime);

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY(Transient) TObjectPtr<UBorder> StormBanner;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StormText;
	UPROPERTY(Transient) TObjectPtr<UBorder> DownBanner;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DownText;
	UPROPERTY(Transient) TObjectPtr<UBorder> ReviveBanner;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> ReviveBar;
	UPROPERTY(Transient) TObjectPtr<UBorder> ScoreBadge;

	float Time = 0.f;
	float ShownStamina = 1.f;
	FString LastScore;
	float ScorePop = 0.f;
};

/**
 * @brief Franja de estado, resultados y feed de mensajes de la partida, construidos en código con el estilo de
 * TN_HUDStyle. Hereda la lógica de UTN_CoopFlowHUDWidget creando los widgets que esa clase enlaza por nombre.
 */
UCLASS()
class TORTUNABO_API UTN_RunFlowHUDWidget : public UTN_CoopFlowHUDWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildTree();

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Canvas;
};
