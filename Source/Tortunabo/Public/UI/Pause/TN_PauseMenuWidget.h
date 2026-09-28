#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TN_PauseMenuWidget.generated.h"

class APlayerState;
class UBorder;
class UCanvasPanel;
class UHorizontalBox;
class UImage;
class UProgressBar;
class UScrollBox;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UWidget;
class UWidgetSwitcher;
class UTN_GameSettingsSubsystem;
class UTN_ScoreShellSynthComponent;

/** Qué es una fila del menú de pausa. */
enum class ETNPauseRowKind : uint8
{
	/** Botón: Intro, Espacio, A del mando o clic. */
	Button,
	/** Deslizador: izquierda y derecha (A y D, cruceta, stick), clic o arrastre sobre la barra. */
	Slider,
	/** Lista de opciones: izquierda y derecha, Intro (la siguiente) o clic en las flechas. */
	Choice,
	/** Solo texto (la lista de controles): se recorre como las demás, no hace nada. */
	Info,
	/** Medidor en vivo (nivel del micrófono con la marca del umbral). */
	Meter,
};

/** Aspecto de una fila. */
enum class ETNPauseRowStyle : uint8
{
	/** Fila de ajustes: panel azul marino a lo ancho, nombre a la izquierda y el control a la derecha. */
	List,
	/** Botón grande de la portada: etiqueta de arena con icono (como los del campeón de la carrera). */
	Big,
	/** Pestaña de los ajustes: píldora (dorada la activa). */
	Tab,
	/** Botón del cuadro de confirmación. */
	Dialog,
};

/** Sonidos de la interfaz (los «pom» y «plin» de las conchas, UTN_ScoreShellSynthComponent). */
enum class ETNPauseSound : uint8
{
	/** Pasar por encima o recorrer con el teclado o el mando. */
	Hover,
	/** Pulsar un botón. */
	Press,
	/** Cambiar un valor (el tono sube con el valor). */
	Tick,
};

/** Páginas del menú de pausa. */
enum class ETNPausePage : uint8
{
	Home,
	Settings,
	Controls,
};

/** Pestañas de los ajustes. */
enum class ETNPauseTab : uint8
{
	Graphics,
	Sound,
	Voice,
	Controls,
	Game,
	Count,
};

/**
 * @brief Fila del menú de pausa hecha en código (botón, deslizador, lista de opciones, texto o medidor), con el estilo
 * del HUD Tortunavy. Se puede enfocar: el ratón la enfoca al pasar por encima, así que teclado, ratón y mando comparten
 * el mismo resaltado. Arriba y abajo los resuelve la navegación de Slate (y el UScrollBox que la contiene la desplaza);
 * izquierda y derecha cambian el valor.
 */
UCLASS()
class TORTUNABO_API UTN_PauseRow : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetupButton(ETNPauseRowStyle InStyle, const FText& InLabel, TFunction<void()> InOnPressed, UTexture2D* InIcon = nullptr,
		const FText& InActionText = FText::GetEmpty());
	void SetupSlider(const FText& InLabel, float InMin, float InMax, float InStep, float InValue, TFunction<FText(float)> InFormat,
		TFunction<void(float)> InOnChanged);
	void SetupChoice(const FText& InLabel, const TArray<FText>& InOptions, int32 InIndex, TFunction<void(int32)> InOnChanged);
	void SetupInfo(const FText& InLabel, const FText& InValue, const FText& InValue2);
	/** Sampler: nivel (0..1), marca (0..1, negativa sin marca) y texto de la derecha; se llama cada fotograma. */
	void SetupMeter(const FText& InLabel, TFunction<void(float& OutLevel, float& OutMark, FText& OutText)> InSampler);

	/** Texto de ayuda que enseña el menú cuando la fila tiene el foco. */
	void SetDescription(const FText& InText) { Description = InText; }
	const FText& GetDescription() const { return Description; }

	/** Apagada: se ve atenuada y no cambia ni se pulsa (se puede seguir recorriendo). */
	void SetRowEnabled(bool bInEnabled);
	bool IsRowEnabled() const { return bEnabled; }

	/** Pestaña activa (píldora dorada). */
	void SetActive(bool bInActive);

	void SetLabel(const FText& InLabel);

	/** Cambian el valor sin avisar (para refrescar la fila desde fuera). */
	void SetSliderValue(float InValue);
	/** Con OverrideText se enseña ese texto en vez de la opción (p. ej. «Personalizada», fuera de la lista). */
	void SetChoiceIndex(int32 InIndex, const FText& OverrideText = FText::GetEmpty());
	void SetChoiceOptions(const TArray<FText>& InOptions, int32 InIndex);

	int32 GetChoiceIndex() const { return Index; }
	float GetSliderValue() const { return Value; }
	ETNPauseRowKind GetKind() const { return Kind; }

	/** Pulsa el botón (o pasa a la siguiente opción) como si se hubiera pulsado Intro. */
	void Activate();

	/** El menú: aviso al recibir el foco (ayuda, desplazamiento) y sonidos. */
	TFunction<void(UTN_PauseRow*)> OnFocused;
	TFunction<void(ETNPauseSound, float)> OnSound;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FNavigationReply NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent,
		const FNavigationReply& InDefaultReply) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UBorder> Frame;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> Sizer;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Value2Text;

	UPROPERTY(Transient)
	TObjectPtr<UImage> IconImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> LeftArrow;

	UPROPERTY(Transient)
	TObjectPtr<UImage> RightArrow;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> BarBox;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Bar;

	UPROPERTY(Transient)
	TObjectPtr<UImage> MarkImage;

	ETNPauseRowKind Kind = ETNPauseRowKind::Info;
	ETNPauseRowStyle Style = ETNPauseRowStyle::List;
	FText Description;
	bool bEnabled = true;
	bool bActive = false;
	bool bFocused = false;
	bool bHovered = false;
	bool bPressed = false;
	bool bDragging = false;
	float Scale = 1.f;

	// Deslizador
	float Min = 0.f;
	float Max = 1.f;
	float Step = 0.05f;
	float Value = 0.f;
	TFunction<FText(float)> Format;
	TFunction<void(float)> OnValueChanged;

	// Lista de opciones
	TArray<FText> Options;
	int32 Index = 0;
	FText ChoiceOverride;
	TFunction<void(int32)> OnChoiceChanged;

	// Botón
	TFunction<void()> OnPressed;

	// Medidor
	TFunction<void(float&, float&, FText&)> Sampler;
	float MeterShown = 0.f;

	/** Monta el árbol de la fila para su tipo y aspecto (lo llaman los Setup). */
	void Build();
	UWidget* BuildListContent();
	UWidget* BuildBigContent();
	void RefreshLook();
	void RefreshValue();
	void StepBy(int32 Direction);
	void SetValueFromScreen(const FVector2D& ScreenPosition);
	void PlaySound(ETNPauseSound Sound, float Pitch = 0.f) const;
};

/** Contador de fotogramas por segundo (ajuste «Mostrar FPS»): abajo a la derecha, sin tapar clics. */
UCLASS()
class TORTUNABO_API UTN_FpsCounterWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FpsText;

	float Accumulated = 0.f;
	int32 Frames = 0;
	float WorstFrame = 0.f;
};

/**
 * @brief Menú de pausa de Tortunavy (todos los modos). No pausa el juego: la partida es en red y sigue.
 *
 * Arriba, el mapa o modo, la sesión y los jugadores conectados con su icono de voz (hablando, silenciado). Portada:
 * Continuar, Ajustes, Controles, Volver al lobby (el anfitrión lleva a todos; un invitado sale él solo al menú
 * principal), Salir de la partida o Menú principal y Salir al escritorio; lo que corta la partida pide confirmación. Ajustes en cinco pestañas
 * (Gráficos, Sonido, Voz, Controles, Juego) que se aplican al momento (UTN_GameSettingsSubsystem y UGameUserSettings);
 * Controles: la lista de teclas del juego, leída de IMC_Player.
 *
 * Mientras está abierto: modo de entrada interfaz y juego, cursor a la vista, la tortuga quieta (sin mover ni girar la
 * cámara) y las teclas se quedan en el menú (salvo la consola y la de pulsar para hablar). Escape o B vuelven atrás (en la
 * portada, cierran); Tabulador o Start cierran del todo; Q y E o LB y RB cambian de pestaña.
 *
 * Lo abre y lo cierra UTN_GameSettingsSubsystem (Escape; Tabulador en el editor; Start del mando).
 */
UCLASS()
class TORTUNABO_API UTN_PauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Toma la entrada del jugador (modo interfaz y juego, cursor, tortuga quieta) y enfoca la primera opción. */
	void TakeInput();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent) override;

private:
	// ── Árbol ────────────────────────────────────────────────────────────────

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> Canvas;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetSwitcher> Pages;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ModeText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SessionText;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> PlayersBox;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> HomeColumn;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HomeNote;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> SettingsList;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> ControlsList;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HelpText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTN_PauseRow>> TabRows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTN_PauseRow>> HomeRows;

	/** Iconos de voz de la lista de jugadores (uno por jugador, en el orden de ChipPlayers). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> ChipVoiceIcons;

	TArray<TWeakObjectPtr<APlayerState>> ChipPlayers;

	// ── Confirmación ─────────────────────────────────────────────────────────

	UPROPERTY(Transient)
	TObjectPtr<UWidget> ConfirmLayer;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ConfirmTitle;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ConfirmText;

	UPROPERTY(Transient)
	TObjectPtr<UTN_PauseRow> ConfirmYes;

	UPROPERTY(Transient)
	TObjectPtr<UTN_PauseRow> ConfirmNo;

	TFunction<void()> ConfirmAction;
	TFunction<void()> CancelAction;
	/** Cuenta atrás del cuadro (resolución nueva: se deshace sola al llegar a cero); negativa sin cuenta. */
	float ConfirmCountdown = -1.f;
	FText ConfirmBaseText;
	TWeakObjectPtr<UTN_PauseRow> FocusBeforeConfirm;

	// ── Estado ───────────────────────────────────────────────────────────────

	ETNPausePage Page = ETNPausePage::Home;
	ETNPauseTab Tab = ETNPauseTab::Graphics;
	TWeakObjectPtr<UTN_PauseRow> LastFocused;
	TWeakObjectPtr<UTN_ScoreShellSynthComponent> Synth;
	double LastSoundTime = 0.0;
	float InfoTimer = 0.f;
	float Clock = 0.f;
	bool bInputTaken = false;
	bool bLeaving = false;
	int32 IgnoreMoveApplied = 0;
	int32 IgnoreLookApplied = 0;
	/** Menús que ya estaban en pantalla al abrir (para saber al cerrar si ha salido otro que quiere el cursor). */
	TArray<TWeakObjectPtr<UUserWidget>> WidgetsAtOpen;
	/** Jugadores con fila propia en la pestaña de voz (para rehacerla si alguien entra o sale). */
	TArray<TWeakObjectPtr<APlayerState>> VoiceTabPlayers;

	/** Filas de gráficos que se refrescan entre sí (calidad general y por partes, escala de resolución, ventana). */
	TWeakObjectPtr<UTN_PauseRow> OverallRow;
	TWeakObjectPtr<UTN_PauseRow> ResolutionRow;
	TWeakObjectPtr<UTN_PauseRow> ResScaleRow;
	TArray<TWeakObjectPtr<UTN_PauseRow>> QualityRows;
	TArray<FIntPoint> ResolutionChoices;

	// ── Montaje ──────────────────────────────────────────────────────────────

	void BuildTree();
	UWidget* BuildHeader();
	UWidget* BuildHomePage();
	UWidget* BuildSettingsPage();
	UWidget* BuildControlsPage();
	UWidget* BuildConfirmLayer();
	void BuildHomeButtons();
	void RefreshHeader();
	void RebuildPlayers();
	void UpdateVoiceIcons();

	void ShowPage(ETNPausePage NewPage);
	void ShowTab(ETNPauseTab NewTab);
	void FillTab();
	void FillGraphicsTab();
	void FillSoundTab();
	void FillVoiceTab();
	void FillControlsTab();
	void FillGameTab();
	void FillControlsList();
	void RefreshGraphicsRows();
	void RefreshHint();

	UTN_PauseRow* NewRow();
	UTN_PauseRow* AddListRow(UScrollBox* List);
	void AddListHeader(UScrollBox* List, const FText& Title);
	void AddListNote(UScrollBox* List, const FText& Note);
	UTN_PauseRow* AddVolumeRow(const FText& Label, const FText& Description, float Value, TFunction<void(float)> OnChanged,
		float MaxValue = 1.f);
	UTN_PauseRow* AddToggleRow(const FText& Label, const FText& Description, bool bValue, TFunction<void(bool)> OnChanged);
	UTN_PauseRow* AddQualityRow(const FText& Label, const FText& Description, int32 Value, TFunction<void(int32)> OnChanged);

	// ── Acciones ─────────────────────────────────────────────────────────────

	void GoBack();
	void CloseMenu();
	void ReturnToLobby();
	void LeaveToMenu();
	void QuitToDesktop();
	void AskConfirm(const FText& Title, const FText& Text, const FText& YesLabel, TFunction<void()> OnYes,
		TFunction<void()> OnNo = nullptr, float CountdownSeconds = -1.f);
	void CloseConfirm(bool bAccepted);
	bool IsConfirmOpen() const;
	void OnVideoModeChanged();

	// ── Entrada y foco ───────────────────────────────────────────────────────

	void ApplyMenuInputMode();
	void ReleaseInput();
	void FocusRow(UTN_PauseRow* Row);
	void FocusFirstOfPage();
	void HandleRowFocused(UTN_PauseRow* Row);
	void PlayUISound(ETNPauseSound Sound, float Pitch);

	// ── Contexto ─────────────────────────────────────────────────────────────

	UTN_GameSettingsSubsystem* GetSettings() const;
	bool IsHost() const;
	bool IsInLobby() const;
	bool CanReturnToLobby() const;
};
