#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "UI/Race/TN_RaceTallyWidget.h"
#include "TN_RaceSprintWidget.generated.h"

class UCanvasPanel;
class UImage;
class UTextBlock;
class UWidget;
class UTN_RaceCueSynthComponent;

/** Lo que enseña el título del sprint final. */
struct FTNRaceSprintSetup
{
	/** Las finalistas (empatadas en lo más alto), con su cara y su nombre. */
	TArray<FTNRaceTallyRow> Finalists;
	/** Medias conchas con las que empatan (6 = tres conchas). */
	int32 TieHalves = 6;
	/** El jugador local corre el sprint (si no, lo mira de fantasma). */
	bool bLocalFinalist = false;
	/** Vista previa por consola (TN.Race.SprintPreview). */
	bool bPreview = false;
};

/** Papelito de confeti o granito de arena que cae sobre el título (se pinta en NativePaint). */
struct FTNSprintConfetti
{
	FVector2D Pos = FVector2D::ZeroVector;
	FVector2D Vel = FVector2D::ZeroVector;
	FVector2D Size = FVector2D(10.f, 16.f);
	float Angle = 0.f;
	float Spin = 0.f;
	float Sway = 0.f;
	FLinearColor Tint = FLinearColor::White;
};

/**
 * @brief Título del sprint final de desempate del modo carrera (ETNBeachRacePhase::SprintIntro), a pantalla completa y
 * hecho en código con el estilo del HUD Tortunavy: fondo azul marino con rayos que giran, «¡SPRINT FINAL!» que entra
 * de golpe y se mece, la cinta con el empate, las caras de las finalistas (su piel, su aro y su nombre) que entran una a
 * una de un «¡pum!» con un «VS» entre ellas, y confeti y arena cayendo. Suena la fanfarria sintetizada
 * (UTN_RaceCueSynthComponent). Abajo, lo que le toca a quien mira (correr o mirarlo de fantasma) y la cuenta de la fase.
 *
 * Lo crea y lo quita UTN_RaceScreensSubsystem (ZOrder 22); también la vista previa TN.Race.SprintPreview.
 */
UCLASS()
class TORTUNABO_API UTN_RaceSprintWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const FTNRaceSprintSetup& InSetup);

	/** Segundos que quedan de la fase (0 = sin cuenta). */
	void SetSecondsLeft(float InSeconds);

	void Dismiss();
	bool IsDismissing() const { return DismissAt >= 0.f; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	void BuildTree();
	void BuildFinalists();
	void SpawnConfetti(const FVector2D& Where, int32 Count, bool bBurst);
	void PlayCue(uint8 Cue, float Pitch, float Volume);

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient) TObjectPtr<UWidget> TitleBox;
	UPROPERTY(Transient) TObjectPtr<UWidget> TieRibbon;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TieText;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> FinalistRow;
	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> SlamWidgets;
	UPROPERTY(Transient) TObjectPtr<UWidget> NoteTag;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> NoteText;
	UPROPERTY(Transient) TObjectPtr<UWidget> CountdownTag;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CountdownText;
	UPROPERTY(Transient) TObjectPtr<UWidget> PreviewTag;

	TWeakObjectPtr<UTN_RaceCueSynthComponent> Synth;
	FTNRaceSprintSetup SprintSetup;
	/** Hora a la que entra de golpe cada cara y cada «VS» (en el orden de SlamWidgets) y si ya ha sonado. */
	TArray<float> SlamAt;
	TArray<bool> SlamDone;
	TArray<FTNSprintConfetti> Confetti;
	FSlateBrush SolidBrush;

	float Time = 0.f;
	float SecondsLeft = 0.f;
	int32 ShownSeconds = -1;
	float ConfettiDebt = 0.f;
	bool bFanfarePlayed = false;
	FVector2D LocalSize = FVector2D(1920.f, 1080.f);
	float DismissAt = -1.f;
};
