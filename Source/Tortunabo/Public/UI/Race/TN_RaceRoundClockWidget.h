#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TN_RaceRoundClockWidget.generated.h"

class UCanvasPanel;
class UTextBlock;
class UWidget;
class UTN_RaceCueSynthComponent;

/** Lo que enseña el reloj del último minuto de la ronda (lo rellena UTN_RaceScreensSubsystem cada fotograma). */
struct FTNRaceRoundClockView
{
	/** Segundos que quedan del tiempo de la ronda (ATN_BeachRaceGameState::GetRoundTimeLeft). */
	float SecondsLeft = 60.f;
	/** Es el sprint final (el aviso dice «gana la más cerca del mar» en vez de «concha para…»). */
	bool bSprint = false;
	/** Vista previa por consola (TN.Race.ClockPreview). */
	bool bPreview = false;
};

/**
 * @brief Reloj del último minuto de la ronda del modo carrera (ATN_BeachRaceGameState::RoundEndServerTime), sin tapar la
 * carrera: una pastilla azul marino arriba en el centro con «TIEMPO DE RONDA» y los segundos (0:59… 0:00),
 * dorada desde los 30 s y coral y latiendo en los 10 últimos, con un «¡toc!» por segundo. Al entrar (a los 60 s) y a los
 * 30 s, una cinta con el aviso («¡Queda 1 minuto!», «¡Quedan 30 segundos!») y, debajo, qué pasa si se acaba: «Si nadie
 * llega al agua, la concha es para la más cerca del mar». Así el «¡TIEMPO!» del final nunca llega por sorpresa.
 *
 * Lo crea y lo quita UTN_RaceScreensSubsystem en Racing mientras el tiempo de la ronda cuenta (nadie ha llegado al agua)
 * y quedan 60 s o menos (ZOrder 14: encima del HUD y debajo de la cuenta atrás tras la primera). No coge el ratón ni el
 * teclado. Vista previa sin jugar: TN.Race.ClockPreview [segundos].
 */
UCLASS()
class TORTUNABO_API UTN_RaceRoundClockWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetView(const FTNRaceRoundClockView& InView);

	/** Se va con un fundido y se quita de la pantalla al acabar. */
	void Dismiss();
	bool IsDismissing() const { return DismissAt >= 0.f; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildTree();
	/** Aviso de la cinta: 1 = queda un minuto, 2 = quedan 30 s. */
	void ShowWarning(int32 Stage);
	void PlayCue(uint8 Cue, float Pitch, float Volume);

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY(Transient) TObjectPtr<UWidget> Pill;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ClockText;
	UPROPERTY(Transient) TObjectPtr<UWidget> Warning;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WarningText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WarningSubText;
	UPROPERTY(Transient) TObjectPtr<UWidget> PreviewTag;

	TWeakObjectPtr<UTN_RaceCueSynthComponent> Synth;
	FTNRaceRoundClockView View;
	bool bHasView = false;

	float Time = 0.f;
	/** Segundos enteros que enseña el reloj (para no reescribirlo) y cuándo cambió (rebote). */
	int32 ShownSeconds = -1;
	float SecondPopAt = -10.f;
	/** Aviso ya enseñado (0 ninguno, 1 el minuto, 2 los 30 s) y desde cuándo se ve la cinta (-1 = no se ve). */
	int32 WarnedStage = 0;
	float WarningAt = -1.f;
	/** Hora a la que empezó a irse (-1 = no se va). */
	float DismissAt = -1.f;
};
