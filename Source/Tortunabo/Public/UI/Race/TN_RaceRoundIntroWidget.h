#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "TN_RaceRoundIntroWidget.generated.h"

class UCanvasPanel;
class UImage;
class UTextBlock;
class UWidget;
class UTN_RaceCueSynthComponent;

/** Lo que enseña el título entre ronda y ronda. */
struct FTNRaceRoundIntroSetup
{
	/** Ronda que empieza (desde 1). */
	int32 Round = 2;
	/** Es el sprint final de desempate: «SPRINT FINAL» en vez de «RONDA N». */
	bool bSprint = false;
	/** Quien gane esta ronda puede coronarse (a alguien le falta una concha o menos). */
	bool bMatchPoint = false;
	/** Frase de ánimo elegida al azar para esta ronda (índice). */
	int32 Quip = 0;
	/** Vista previa por consola (TN.Race.RoundPreview): lo dice una etiqueta. */
	bool bPreview = false;

	bool operator==(const FTNRaceRoundIntroSetup& Other) const
	{
		return Round == Other.Round && bSprint == Other.bSprint && bMatchPoint == Other.bMatchPoint && Quip == Other.Quip && bPreview == Other.bPreview;
	}
	bool operator!=(const FTNRaceRoundIntroSetup& Other) const { return !(*this == Other); }
};

/** Trocito de cáscara que salta al entrar el título (se pinta en NativePaint). */
struct FTNRoundIntroBit
{
	FVector2D Pos = FVector2D::ZeroVector;
	FVector2D Vel = FVector2D::ZeroVector;
	float Size = 10.f;
	float Angle = 0.f;
	float Spin = 0.f;
	float Age = 0.f;
	float Life = 1.2f;
	FLinearColor Tint = FLinearColor::White;
};

/**
 * @brief Título entre ronda y ronda del modo carrera (Docs/Modo_Carrera.md, «Entre ronda y ronda»), encima de la cáscara
 * oscura cerrada (UTN_GhostHatchWidget en modo carrera): «RONDA N» (o «SPRINT FINAL» en el desempate) entra de golpe con un
 * «¡pum!» y trocitos de cáscara, debajo una frase de ánimo (o «¡Esta ronda puede coronar a una campeona!»), «Colocando la
 * playa…» mientras el servidor prepara la ronda y, cuando las tortugas ya están en sus huevos, el 3, 2, 1 de la salida en
 * un medallón, cada número con su «pum» de la cáscara. Al dar la salida la cáscara se rompe y el título sale disparado:
 * la tortuga ya está saltando de su huevo (con la pausa de 1 s del huevo de siempre).
 *
 * Lo pone y lo quita UTN_RaceScreensSubsystem (ZOrder de la cáscara + 1). No coge ratón ni teclado.
 */
UCLASS()
class TORTUNABO_API UTN_RaceRoundIntroWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Lo que enseña. Se puede repetir antes de Play (la ronda puede llegar por red un poco después que la fase). */
	void Setup(const FTNRaceRoundIntroSetup& InSetup);

	/** Entra el título (con la cáscara ya cerrada debajo). Mientras no se llama, no se ve. */
	void Play();
	bool IsPlaying() const { return PlayAt >= 0.f; }

	/** Número de la cuenta de salida (3, 2, 1) con su golpe. */
	void ShowCount(int32 Number);

	/** Se va ya (el título sale disparado y se funde) y se quita sola. */
	void Dismiss();
	bool IsDismissing() const { return DismissAt >= 0.f; }

	/** Cuántas frases de ánimo hay para las rondas normales (se elige una al azar por ronda). */
	static int32 NumQuips();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	void BuildTree();
	void ApplyTexts();
	void SpawnBits(const FVector2D& Where, int32 Num);
	void PlayCue(uint8 Cue, float Pitch, float Volume);

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient) TObjectPtr<UWidget> QuipTag;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> QuipText;
	UPROPERTY(Transient) TObjectPtr<UWidget> CountBox;
	UPROPERTY(Transient) TObjectPtr<UImage> CountDisc;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CountText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UWidget> PreviewTag;

	TWeakObjectPtr<UTN_RaceCueSynthComponent> Synth;
	FTNRaceRoundIntroSetup IntroSetup;
	bool bHasSetup = false;
	TArray<FTNRoundIntroBit> Bits;
	FSlateBrush SolidBrush;

	float Time = 0.f;
	float PlayAt = -1.f;
	float CountAt = -1.f;
	float DismissAt = -1.f;
	int32 Count = 0;
	bool bSlamPlayed = false;
	FVector2D LocalSize = FVector2D(1920.f, 1080.f);
};
