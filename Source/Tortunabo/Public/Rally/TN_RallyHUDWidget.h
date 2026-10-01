// HUD del Rally en C++ (WidgetTree construido en código con TN_RaceUIKit): velocidad, puesto, vuelta y puerta, semáforo,
// contramano, reaparición, munición especial, punto de mira de la artillera, calor de la torreta, tinta, cuenta atrás de
// cierre y tabla de resultados. Lee el estado replicado (ATN_RallyGameState); no coge el ratón ni el teclado.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Rally/TN_RallyGameState.h"
#include "TN_RallyHUDWidget.generated.h"

class ATN_Buggy;
class UBorder;
class UCanvasPanel;
class UImage;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class UWidget;

UCLASS()
class TORTUNABO_API UTN_RallyHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Calor de la torreta (0 frío .. 1 sobrecalentada). Lo pasa el buggy del jugador local. */
	UFUNCTION(BlueprintCallable, Category = "Rally")
	void SetTurretHeat(float Heat01);

	/** Segundos que le quedan a la tinta en pantalla (0 = limpia). Lo pasa el buggy del jugador local. */
	UFUNCTION(BlueprintCallable, Category = "Rally")
	void SetInkSeconds(float SecondsLeft);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildTree();
	void BuildInk();
	void BuildResults();
	void Refresh(const ATN_RallyGameState& RallyState);
	void RefreshSemaphore(const ATN_RallyGameState& RallyState, double ServerTime);
	void RefreshStatus(const ATN_RallyGameState& RallyState, double ServerTime, const FTNRallyStanding* Mine);
	void RefreshResults(const ATN_RallyGameState& RallyState, double ServerTime);
	void RefreshTurret();

	/**
	 * Buggy del jugador local: el que posee como conductora o el de su peón de artillera; si no, el de su fila de
	 * puestos. Nullptr si mira la carrera sin plaza.
	 */
	const ATN_Buggy* FindLocalBuggy() const;

	/** Lee del buggy local el calor, la munición especial y la tinta (estado replicado; vale en cliente y servidor). */
	void PullFromLocalBuggy();

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PlaceText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LapText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SpeedText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SpeedUnitText;
	UPROPERTY(Transient) TObjectPtr<UWidget> SemaphoreBox;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> SemaphoreLights;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CenterText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WrongWayText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RespawnText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AmmoText;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> HeatBar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HeatLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Crosshair;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> InkSplats;
	UPROPERTY(Transient) TObjectPtr<UBorder> ResultsPanel;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> ResultsRows;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResultsFooter;

	float TurretHeat = 0.f;
	bool bTurretOverheated = false;
	int32 SpecialCharges = 0;
	float InkSeconds = 0.f;
	float TextAccumulator = 1.f;
	/** Firma de la tabla pintada (para no reconstruir las filas cada fotograma). */
	uint32 ShownResultsHash = 0;
};
