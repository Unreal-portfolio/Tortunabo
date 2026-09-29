#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "TN_RaceArrivalWidget.generated.h"

class UCanvasPanel;
class UImage;
class UTextBlock;
class UWidget;
class UTN_RaceCueSynthComponent;
class UTN_ScoreShellSynthComponent;

/** Lo que enseña la pantalla del puesto al llegar al agua. */
struct FTNRaceArrivalSetup
{
	/** Puesto en la ronda (1-8), el que ha decidido el servidor (ATN_BeachRaceGameState::RoundArrivals). */
	int32 Place = 1;
	/** Ronda (desde 1) y si es el sprint final: lo dice la cinta de arriba. */
	int32 Round = 1;
	bool bSprint = false;
	/** Vista previa por consola (TN.Race.ArrivalPreview): lo dice una etiqueta. */
	bool bPreview = false;
};

/** Papelito de confeti, destello, grano de arena o gota de lluvia (se pinta en NativePaint). */
struct FTNArrivalParticle
{
	FVector2D Pos = FVector2D::ZeroVector;
	FVector2D Vel = FVector2D::ZeroVector;
	FVector2D Size = FVector2D(10.f, 16.f);
	float Angle = 0.f;
	float Spin = 0.f;
	float Age = 0.f;
	float Life = 3.f;
	/** 0 papelito de confeti, 1 destello de cuatro puntas, 2 gota de lluvia (raya), 3 grano de arena o polvo que salta. */
	uint8 Kind = 0;
	FLinearColor Tint = FLinearColor::White;
};

/**
 * @brief Pantalla del puesto al llegar al agua de meta en el modo carrera (Docs/Modo_Carrera.md, «Llegada al agua»), solo
 * en la pantalla de quien llega y encima de la cáscara oscura cerrada (UTN_GhostHatchWidget en modo carrera): arriba la
 * cinta de la ronda, «HAS QUEDADO» y el puesto enorme («4.º», del color de su medalla); en medio, su premio dibujado en
 * código (TN_RaceArrivalArt.h), que cae y rebota; debajo, el nombre del premio y un mensaje gracioso elegido al azar entre
 * los de su puesto (alentadores en el podio, cada vez más burlones abajo).
 *
 * 1.º corona de oro enorme con rayos dorados, confeti y fanfarria; 2.º corona de plata más pequeña y 3.º corona de bronce
 * enana, con destellos y el «¡plin!» de las conchas; 4.º un cubo de playa del revés por corona («pom… pom»); 5.º media
 * concha rota; 6.º flotador pinchado; 7.º calcetín mojado con arena; 8.º alga de peluca; del 5.º al 8.º con el trombón
 * triste (más grave cuanto peor) y, del 6.º, una nubecita que les llueve encima.
 *
 * Dura ContentSeconds desde Play, con su salida rápida al final; la pone y la quita UTN_RaceScreensSubsystem (ZOrder de la
 * cáscara + 1), que abre la cáscara al acabar. No coge ratón ni teclado.
 */
UCLASS()
class TORTUNABO_API UTN_RaceArrivalWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Segundos que dura la pantalla desde Play, incluida su salida (≈2,5-3 s con el cierre de la cáscara). */
	static constexpr float ContentSeconds = 2.7f;

	/** Empieza a enseñar el puesto (con la cáscara ya cerrada debajo). Mientras no se llama, no se ve. */
	void Play(const FTNRaceArrivalSetup& InSetup);
	bool IsPlaying() const { return bPlaying; }

	/** Ha pasado su tiempo (ya se ha ido con su salida). */
	bool IsDone() const { return bPlaying && Time >= ContentSeconds; }

	/** Segundos desde que acabó (0 si aún no). */
	float GetTimeSinceDone() const { return IsDone() ? Time - ContentSeconds : 0.f; }

	/** Se va ya con un fundido corto y se quita sola. */
	void Dismiss();
	bool IsDismissing() const { return DismissAt >= 0.f; }

	/** Mensajes graciosos de cada puesto (1-8; del 8.º en adelante, los del 8.º). Se elige uno al azar en cada llegada. */
	static TArray<FText> MessagesFor(int32 Place);

	/** Nombre del premio de cada puesto («Corona de oro», «Flotador pinchado»...). */
	static FText PrizeNameFor(int32 Place);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	void BuildTree();
	/** Lo de cada momento: golpe del puesto, caída del premio con su sonido y las partículas. */
	void FireCues();
	void AnimatePrize(float T);
	void SpawnParticles(const FVector2D& Where, int32 Count, uint8 Kind, bool bBurst);
	void PlayCue(uint8 Cue, float Pitch, float Volume);
	void PlayShell(bool bPlin, uint8 Tier, float Semitones, float Volume);

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY(Transient) TObjectPtr<UWidget> RoundRibbon;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RoundText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HeadText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PlaceText;
	UPROPERTY(Transient) TObjectPtr<UImage> PrizeImage;
	UPROPERTY(Transient) TObjectPtr<UWidget> PrizeBox;
	UPROPERTY(Transient) TObjectPtr<UWidget> PrizeTag;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PrizeText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(Transient) TObjectPtr<UWidget> PreviewTag;

	TWeakObjectPtr<UTN_RaceCueSynthComponent> CueSynth;
	TWeakObjectPtr<UTN_ScoreShellSynthComponent> ShellSynth;
	FTNRaceArrivalSetup ArrivalSetup;
	TArray<FTNArrivalParticle> Particles;
	FSlateBrush SolidBrush;
	FSlateBrush GlowBrush;
	FSlateBrush SparkBrush;
	FSlateBrush CloudBrush;

	float Time = 0.f;
	float DismissAt = -1.f;
	bool bPlaying = false;
	/** Cuántos avisos del guion ya han sonado (golpe del puesto, caída del premio, remate). */
	int32 CuesFired = 0;
	float RainDebt = 0.f;
	float ConfettiDebt = 0.f;
	float SparkDebt = 0.f;
	FVector2D LocalSize = FVector2D(1920.f, 1080.f);
	/** Centro del premio en el espacio del widget (lo mira el pintado: rayos, brillo y nube). */
	FVector2D PrizeCenter = FVector2D(960.f, 620.f);
	FVector2D PrizeExtent = FVector2D(320.f, 320.f);
};
