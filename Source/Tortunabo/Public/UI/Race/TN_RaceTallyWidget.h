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

/** Un jugador en el recuento o en el podio: nombre, aspecto (cosméticos) y conchas (en medias). */
struct FTNRaceTallyRow
{
	FString Name;
	FTN_TurtleLook Look;
	/** Medias conchas al empezar el recuento, sin las de esta ronda (2 = una concha entera). */
	int32 HalvesBefore = 0;
	/** Medias que gana en esta ronda: 2 la primera en el agua, 1 las que llegan en la cuenta atrás, 0 las demás. */
	int32 HalvesGained = 0;
	/** Es el jugador de esta máquina («TÚ»). */
	bool bLocal = false;
};

/** Lo que enseña el recuento de una ronda (lo rellena UTN_RaceScreensSubsystem o la vista previa por consola). */
struct FTNRaceTallySetup
{
	TArray<FTNRaceTallyRow> Rows;
	/** Columna de la primera en el agua, a la que vuela su concha entera; INDEX_NONE si nadie llegó. */
	int32 WinnerRow = INDEX_NONE;
	/** Columna de la campeona (la única en lo más alto con Target conchas o más): le baja la corona. */
	int32 ChampionRow = INDEX_NONE;
	/** Columnas empatadas en lo más alto con Target conchas o más: sprint final entre ellas. */
	TArray<int32> SprintRows;
	/** Conchas para ser campeón (huecos de cada columna; cada hueco, dos medias). */
	int32 Target = 3;
	/** Ronda que se acaba de jugar (desde 1). */
	int32 Round = 1;
	/** Las conchas de esta ronda ya se vieron llegar en un recuento anterior: salen ya puestas y se celebra. */
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
	/** Por hueco: la concha entera y sus dos mitades (la de arriba a la izquierda y la que encaja con ella). */
	TArray<TWeakObjectPtr<UImage>> Shells;
	TArray<TWeakObjectPtr<UImage>> HalvesA;
	TArray<TWeakObjectPtr<UImage>> HalvesB;
	/** Por hueco: medias que enseña (0, 1 o 2), cuántas enseñaba antes y desde cuándo (hora del recuento). */
	TArray<int32> SocketLevel;
	TArray<int32> SocketFrom;
	TArray<float> SocketChangedAt;
	/** Hora a la que aparece cada concha entera que ya tenía (y la media, si le sobraba una). */
	TArray<float> BeforeFullAt;
	float BeforeHalfAt = -1.f;
	/** Orden de su media concha de esta ronda entre las que la ganan (-1 = no gana media). */
	int32 HalfOrder = INDEX_NONE;
	/** Medias que enseñaba el fotograma anterior y qué conchas de esta ronda ya han llegado. */
	int32 ShownHalves = 0;
	bool bWinnerCounted = false;
	bool bHalfCounted = false;
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
 * encima, los huecos de concha en zigzag unidos por una cuerda, hasta la corona del campeón. Cada hueco guarda dos medias
 * conchas: las conchas que ya tenía aparecen una a una con un «pom» que sube por la escala (la media, partida en
 * diagonal, con un «pom» más bajo); luego la concha entera de la ronda nace en el centro, gira y vuela en arco hasta su
 * hueco en la columna de la primera en el agua, donde cae con «¡plin!», rebote y destellos, y su cara celebra con ojos de
 * estrella. Después, una a una, las medias conchas de las que llegaron en la cuenta atrás: saltan a su hueco con un
 * «plin» pequeño y, si completan una concha, la otra mitad encaja con la suya y se vuelve entera. Si nadie llegó al agua,
 * lo dice el cartel de arriba y las caras se marean. Abajo, la cuenta atrás de la fase (PhaseSecondsLeft). Con la
 * campeona, le baja la corona; con empate en lo más alto, el cartel anuncia el sprint final.
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

	/** true cuando ya se ha visto todo (las conchas en su hueco y la celebración): el campeón puede tomar el relevo. */
	bool IsSequenceDone() const { return Time >= DoneAt; }

	/** Alguien acaba campeona en este recuento (le baja la corona). */
	bool IsChampionTally() const;

	/** Hay empate en lo más alto: después viene el sprint final. */
	bool IsSprintTally() const { return TallySetup.SprintRows.Num() > 1; }

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
	void TickSockets(int32 ColumnIndex);
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
	/** Alguna gana media concha en esta ronda. */
	bool HasHalves() const { return NumHalfRows > 0; }
	/** Hueco (desde 0, abajo) en el que cae la concha entera de la ronda (el de su primera media). */
	int32 NewSlot() const;
	/** Medias que enseña ahora la columna (las que tenía según van saliendo, la concha de la ronda y su media). */
	int32 HalvesShownAt(int32 ColumnIndex) const;
	/** Hora a la que llega la media concha de la columna (o -1). */
	float HalfLandAt(int32 ColumnIndex) const;
	/** Nombres de una lista de columnas: «A», «A y B», «A, B y C». */
	FText JoinNames(const TArray<int32>& RowIndices) const;

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
	/** Desde cuándo saltan las medias conchas (una cada HalfStagger) y cuándo ha llegado la última. */
	float HalvesFromAt = 3.f;
	float FinalAt = 3.f;
	/** Cuándo baja la corona de la campeona o se anuncia el sprint final. */
	float VerdictAt = 3.5f;
	float DoneAt = 5.f;
	int32 NumHalfRows = 0;
	bool bLanded = false;
	bool bAppeared = false;
	bool bCrownSounded = false;
	/** Nadie ganó: los dos «pom» que bajan (el segundo, a esta hora; negativa = ya sonó). */
	float NobodyPomAt = -1.f;
	int32 PomStep = 0;
	/** Resultado que enseña el cartel de arriba (0 recuento, 1 la ronda, 2 campeona o sprint) para no reescribirlo. */
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
