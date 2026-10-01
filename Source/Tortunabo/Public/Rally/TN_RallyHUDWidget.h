// HUD del Rally en C++ (WidgetTree construido en código con TN_RaceUIKit): velocidad, turbo, puesto, vuelta y puerta,
// semáforo (el único temporizador de la salida), contramano, reaparición y su aviso, munición seleccionada con sus cargas,
// vida del buggy, noqueo de la artillera, punto de mira de la artillera, calor de la torreta, tinta, cuenta atrás de cierre
// y tabla de resultados. Lee el estado replicado
// (ATN_RallyGameState); no coge el ratón ni el teclado.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Rally/TN_RallyGameState.h"
#include "Rally/TN_RallyVehicle.h"
#include "TN_RallyHUDWidget.generated.h"

class ATN_Buggy;
class UBorder;
class UCanvasPanel;
class UImage;
class UProgressBar;
class USoundBase;
class UTextBlock;
class UVerticalBox;
class UWidget;

UCLASS()
class TORTUNABO_API UTN_RallyHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTN_RallyHUDWidget(const FObjectInitializer& ObjectInitializer);

	/** Pitido de cada luz roja del semáforo y sonido del verde (2D, para el jugador local). */
	UPROPERTY(EditDefaultsOnly, Category = "Rally|Sonido")
	TObjectPtr<USoundBase> LightBeepSound;

	UPROPERTY(EditDefaultsOnly, Category = "Rally|Sonido")
	TObjectPtr<USoundBase> LightGoSound;

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
	/** Semáforo de la salida (tres luces en un panel, arriba en el centro). */
	void BuildSemaphore();
	/** Torreta abajo a la izquierda: munición, calentamiento, vida del buggy y aviso de noqueo. */
	void BuildTurretPanel();
	void BuildInk();
	void BuildResults();
	void Refresh(const ATN_RallyGameState& RallyState);
	void RefreshSemaphore(const ATN_RallyGameState& RallyState, double ServerTime);
	/** Puesto y vuelta (o puerta) de la fila propia. */
	void RefreshPlace(const ATN_RallyGameState& RallyState, const FTNRallyStanding* Mine);
	void RefreshStatus(const ATN_RallyGameState& RallyState, double ServerTime, const FTNRallyStanding* Mine);
	void RefreshResults(const ATN_RallyGameState& RallyState, double ServerTime);
	void RefreshTurret();
	/** Barra del turbo (carga del buggy local; resaltada mientras empuja). */
	void RefreshBoost(bool bVisible);
	/** Munición seleccionada de la torreta (y la especial en reserva si va con el coco), para la conductora y la artillera. */
	void RefreshAmmo(bool bVisible);
	/** Vida del buggy propio: barra pequeña y discreta junto a la de la torreta. */
	void RefreshHealth(bool bVisible);
	/** «¡Noqueada!» con la cuenta, solo para la artillera mientras dura el noqueo. */
	void RefreshKnock(bool bGunner);
	/** Aviso «Mantén R…» la primera vez que el buggy local reaparece (LastRespawnServerTime de su fila). */
	void RefreshRespawnHint(const FTNRallyStanding* Mine, double ServerTime);

	/**
	 * Buggy del jugador local: el que posee como conductora o el de su peón de artillera; si no, el de su fila de
	 * puestos. Nullptr si mira la carrera sin plaza.
	 */
	const ATN_Buggy* FindLocalBuggy() const;

	/** Lee del buggy local el calor, la munición, la vida, el noqueo y la tinta (estado replicado; vale en cliente y servidor). */
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
	UPROPERTY(Transient) TObjectPtr<UProgressBar> HealthBar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HealthLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> KnockText;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> BoostBar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BoostLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RespawnHintText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Crosshair;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> InkSplats;
	UPROPERTY(Transient) TObjectPtr<UBorder> ResultsPanel;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> ResultsRows;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResultsFooter;

	float TurretHeat = 0.f;
	bool bTurretOverheated = false;
	/** Último paso del semáforo que ha sonado: 0 = ninguno, 1-3 = luces rojas, 4 = verde. */
	int32 SemaphoreStepHeard = 0;
	int32 SpecialCharges = 0;
	ETNRallyAmmo SpecialAmmo = ETNRallyAmmo::None;
	ETNRallyAmmo SelectedAmmo = ETNRallyAmmo::Coco;
	float Health01 = 1.f;
	float GunnerKnockSeconds = 0.f;
	/** El buggy local lleva artillera: entonces la conductora no dispara y su HUD no enseña munición ni calor. */
	bool bBuggyHasGunner = false;
	float InkSeconds = 0.f;
	float BoostCharge = 0.f;
	bool bBoosting = false;
	/** Última reaparición vista en la fila propia (hora del servidor) y fin del aviso en pantalla. */
	float SeenRespawnServerTime = 0.f;
	double RespawnHintEndServerTime = 0.0;
	bool bRespawnHintShown = false;
	float TextAccumulator = 1.f;
	/** Firma de la tabla pintada (para no reconstruir las filas cada fotograma). */
	uint32 ShownResultsHash = 0;
};
