#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/TN_CosmeticsTypes.h"
#include "Styling/SlateBrush.h"
#include "TN_RaceTallyWidget.generated.h"

class UCanvasPanel;
class UHorizontalBox;
class UImage;
class UTextBlock;
class UWidget;
class UTN_ScoreShellSynthComponent;

/** Un jugador en el recuento o en el podio: nombre, aspecto (cosméticos) y conchas que tenía antes de esta ronda. */
struct FTNRaceTallyRow
{
	FString Name;
	FTN_TurtleLook Look;
	/** Conchas llenas al empezar el recuento (sin la que se gana en esta ronda). */
	int32 WinsBefore = 0;
	/** Es el jugador de esta máquina («TÚ»). */
	bool bLocal = false;
};

/** Lo que enseña el recuento de una ronda (lo rellena UTN_RaceScreensSubsystem o la vista previa por consola). */
struct FTNRaceTallySetup
{
	TArray<FTNRaceTallyRow> Rows;
	/** Columna del ganador de la ronda, a la que vuela su concha nueva; INDEX_NONE si nadie llegó al agua. */
	int32 WinnerRow = INDEX_NONE;
	/** Conchas para ser campeón (huecos de cada columna). */
	int32 Target = 3;
	/** Ronda que se acaba de jugar (desde 1). */
	int32 Round = 1;
	/** La concha de esta ronda ya se vio volar en un recuento anterior: sale ya puesta y el ganador celebra. */
	bool bAlreadyLanded = false;
	/** Vista previa por consola (lo dice una etiqueta). */
	bool bPreview = false;
};

/** Partícula de los destellos del recuento (se pinta en NativePaint). */
struct FTNTallySpark
{
	FVector2D Pos = FVector2D::ZeroVector;
	FVector2D Vel = FVector2D::ZeroVector;
	float Age = 0.f;
	float Life = 0.6f;
	float Size = 24.f;
	float Angle = 0.f;
	float Spin = 0.f;
	FLinearColor Tint = FLinearColor::White;
};

/** Una columna del recuento: sus widgets y cómo va su animación. */
struct FTNTallyColumn
{
	TWeakObjectPtr<UWidget> Root;
	TWeakObjectPtr<UImage> Face;
	TWeakObjectPtr<UImage> Glow;
	TWeakObjectPtr<UImage> Crown;
	TArray<TWeakObjectPtr<UImage>> Sockets;
	TArray<TWeakObjectPtr<UImage>> Shells;
	/** Hora del recuento en que aparece cada concha llena (negativa = hueco vacío); la de la ronda, al llegar volando. */
	TArray<float> ShellTimes;
	/** Qué conchas ya han sonado al aparecer. */
	TArray<bool> ShellSounded;
	/** Cuándo entra la columna (hora del recuento). */
	float InAt = 0.f;
	/** Cara que enseña (ETNTurtleFace) y su rebote al cambiar. */
	uint8 FaceShown = 0xFF;
	float FacePop = 0.f;
	/** Brillos alrededor de la cara mientras celebra: cuándo sale el siguiente. */
	float NextSparkle = 0.f;
};

/**
 * @brief Recuento de conchas tras cada ronda del modo carrera (ETNBeachRacePhase::RoundResults), a pantalla completa y
 * con el estilo del HUD Tortunavy, hecho en código.
 *
 * Una columna por jugador, como en los concursos de preguntas en los que se va subiendo: su cara de tortuga con su
 * color de piel (TNRaceArt::TurtleFaceFor) en un aro del color de su caparazón, su nombre en una etiqueta de arena y,
 * encima, los huecos de concha en zigzag unidos por una cuerda, hasta la corona del campeón. Las conchas que ya tenía
 * aparecen una a una con un «pom» que sube por la escala; luego la concha de la ronda nace en el centro, gira y vuela
 * en arco hasta su hueco en la columna del ganador, donde cae con «¡plin!», rebote y destellos, y su cara celebra con
 * ojos de estrella. Si nadie llegó al agua, lo dice el cartel de arriba y las caras se marean. Abajo, la cuenta atrás de
 * la fase (PhaseSecondsLeft). La concha que completa el objetivo corona al campeón: la corona baja a su cabeza.
 *
 * Lo crea y lo quita UTN_RaceScreensSubsystem; también la vista previa TN.Race.Tally.
 */
UCLASS()
class TORTUNABO_API UTN_RaceTallyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const FTNRaceTallySetup& InSetup);

	/** Mientras la concha nueva no haya salido volando, cambia la columna del ganador (el dato puede llegar tarde). */
	void UpdateWinner(int32 InWinnerRow);

	/** Segundos que quedan de la fase (0 = sin cuenta atrás). */
	void SetSecondsLeft(float InSeconds);

	/** true cuando ya se ha visto todo (la concha en su hueco y la celebración): el campeón puede tomar el relevo. */
	bool IsSequenceDone() const { return Time >= DoneAt; }

	/** La concha de la ronda completa el objetivo (el ganador es campeón). */
	bool IsChampionTally() const;

	/** Se va con un fundido y se quita de la pantalla al acabar. */
	void Dismiss();
	bool IsDismissing() const { return DismissAt >= 0.f; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	void BuildTree();
	void BuildColumns();
	void PlanTimeline();
	void TickColumns(float DeltaTime);
	void TickFlight(const FGeometry& MyGeometry);
	void TickTexts();
	void TickBubbles();
	void TickSparks(float DeltaTime);
	void Land();
	void Burst(const FVector2D& Where, int32 Count, float Speed, float Size);
	void PlaySound(bool bPlin, uint8 Tier, float Semitones, float Volume);
	/** Centro de un widget en el espacio de este (false si aún no se ha colocado). */
	bool CenterOf(const UWidget* Widget, const FGeometry& MyGeometry, FVector2D& OutCenter) const;
	bool HasWinner() const { return TallySetup.Rows.IsValidIndex(TallySetup.WinnerRow); }
	/** Hueco (desde 0, abajo) al que va la concha de la ronda. */
	int32 NewSlot() const;

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> BubbleLayer;
	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> ColumnBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RoundText;
	UPROPERTY(Transient) TObjectPtr<UWidget> ResultCard;
	UPROPERTY(Transient) TObjectPtr<UImage> ResultFace;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResultText;
	UPROPERTY(Transient) TObjectPtr<UWidget> CountdownTag;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CountdownText;
	UPROPERTY(Transient) TObjectPtr<UWidget> PreviewTag;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> Bubbles;

	FTNRaceTallySetup TallySetup;
	TArray<FTNTallyColumn> Columns;
	TArray<FTNTallySpark> Sparks;
	TWeakObjectPtr<UTN_ScoreShellSynthComponent> Synth;

	/** Pinceles de NativePaint: la concha que vuela, su brillo y los destellos. */
	FSlateBrush FlyingBrush;
	FSlateBrush GlowBrush;
	FSlateBrush SparkBrush;

	float Time = 0.f;
	float SecondsLeft = 0.f;
	int32 ShownSeconds = -1;
	/** Guion (hora del recuento, s): nace la concha nueva, sale volando, llega y ya se ha visto todo. */
	float AppearAt = 1.35f;
	float LaunchAt = 1.9f;
	float LandAt = 2.55f;
	float DoneAt = 5.f;
	bool bLanded = false;
	bool bAppeared = false;
	bool bCrownSounded = false;
	/** Nadie ganó: los dos «pom» que bajan (el segundo, a esta hora; negativa = ya sonó). */
	float NobodyPomAt = -1.f;
	int32 PomStep = 0;
	/** Resultado que enseña el cartel de arriba (0 recuento, 1 ganador o nadie) para no reescribirlo cada fotograma. */
	int32 ResultShown = -1;
	/** Concha que vuela: de dónde sale, dónde está, su tamaño, su giro, su estela y su brillo (espacio de este widget). */
	FVector2D FlyFrom = FVector2D::ZeroVector;
	FVector2D FlyPos = FVector2D::ZeroVector;
	FVector2D FlyTrailA = FVector2D::ZeroVector;
	FVector2D FlyTrailB = FVector2D::ZeroVector;
	float FlySize = 0.f;
	float FlyAngle = 0.f;
	float FlyGlow = 0.f;
	bool bFlyVisible = false;
	bool bFlying = false;
	/** Tamaño de este widget (espacio local) del último fotograma. */
	FVector2D LocalSize = FVector2D(1920.f, 1080.f);
	/** Hora a la que empezó a irse (-1 = no se va). */
	float DismissAt = -1.f;
};
