#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/TN_CosmeticsTypes.h"
#include "Game/TN_BeachRaceGameState.h"
#include "TN_RaceFinishCountdownWidget.generated.h"

class UCanvasPanel;
class UImage;
class UTextBlock;
class UWidget;
class UTN_RaceCueSynthComponent;

/** Lo que enseña la cuenta atrás tras la primera en el agua (lo rellena UTN_RaceScreensSubsystem cada fotograma). */
struct FTNRaceCountdownView
{
	ETNBeachFinishCountdown State = ETNBeachFinishCountdown::None;
	float SecondsLeft = 0.f;
	float TotalSeconds = 10.f;
	/** La primera en el agua (se lleva la concha entera). */
	FString LeaderName;
	FTN_TurtleLook LeaderLook;
	/** Qué le toca al jugador local: 0 sigue corriendo, 1 es la primera, 2 llegó en la cuenta (media), 3 solo mira. */
	uint8 LocalStatus = 0;
	/** Vista previa por consola (TN.Race.CountdownPreview). */
	bool bPreview = false;
};

/**
 * @brief Cuenta atrás grande tras la primera tortuga en el agua de meta (modo carrera, ETNBeachFinishCountdown), sin
 * tapar la carrera: arriba, la cinta «¡La primera ya está en el agua!» con su cara y quién se lleva la concha; en medio,
 * el número (10, 9, 8… 1) en un medallón que late, se pone dorado y luego coral y tiembla al final; debajo, lo que te
 * toca («¡Corre! Media concha si llegas», «¡Concha entera para ti!», «¡Media concha para ti!»). Suena un «¡toc!» de caja
 * china cada segundo que se acelera (cada medio segundo desde los 5 y cada cuarto desde los 2,5). Al acabar, «¡TIEMPO!»
 * (o «¡TODAS AL AGUA!») con el silbato del árbitro.
 *
 * Lo crea y lo quita UTN_RaceScreensSubsystem en Racing (ZOrder 15: encima del HUD y debajo del recuento); también la
 * vista previa TN.Race.CountdownPreview. No coge el ratón ni el teclado.
 */
UCLASS()
class TORTUNABO_API UTN_RaceFinishCountdownWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetView(const FTNRaceCountdownView& InView);

	/** Se va con un fundido y se quita de la pantalla al acabar. */
	void Dismiss();
	bool IsDismissing() const { return DismissAt >= 0.f; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildTree();
	void RefreshTexts();
	void PlayCue(uint8 Cue, float Pitch, float Volume);

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY(Transient) TObjectPtr<UWidget> Banner;
	UPROPERTY(Transient) TObjectPtr<UImage> LeaderFace;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BannerText;
	UPROPERTY(Transient) TObjectPtr<UWidget> SubTag;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SubText;
	UPROPERTY(Transient) TObjectPtr<UWidget> Medal;
	UPROPERTY(Transient) TObjectPtr<UImage> MedalGlow;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> NumberText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TimeUpText;
	UPROPERTY(Transient) TObjectPtr<UWidget> StatusTag;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UWidget> PreviewTag;

	TWeakObjectPtr<UTN_RaceCueSynthComponent> Synth;
	FTNRaceCountdownView View;
	bool bHasView = false;

	float Time = 0.f;
	/** Segundos que quedaban el fotograma anterior (para los «¡toc!») y el número que se enseña. */
	float LastLeft = -1.f;
	int32 ShownNumber = -1;
	float NumberPopAt = -10.f;
	/** Desde cuándo se enseña «¡TIEMPO!» (-1 = aún no). */
	float TimeUpAt = -1.f;
	/** Estado y textos pintados (para no reescribirlos cada fotograma). */
	ETNBeachFinishCountdown ShownState = ETNBeachFinishCountdown::None;
	uint8 ShownStatus = 0xFF;
	FString ShownLeader;
	float DismissAt = -1.f;
};
