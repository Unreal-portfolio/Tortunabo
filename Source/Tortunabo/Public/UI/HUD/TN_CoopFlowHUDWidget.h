#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/TN_MatchFlowTypes.h"
#include "TN_CoopFlowHUDWidget.generated.h"

class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UWidget;
class ATN_CoopGameState;
class ATN_CoopPlayerState;

UCLASS()
class TORTUNABO_API UTN_CoopFlowHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ── Lobby / In-progress status strip ──────────────────────────────────────
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> RootContainer;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PrimaryText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SecondaryText;

	// ── Results panel widgets — name them EXACTLY as declared here ─────────────
	// Any container widget (Overlay, Border, CanvasPanel…) named "ResultsOverlay"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ResultsOverlay;

	// "¡PRIMER LUGAR!" / "¡ELIMINADO!" etc.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResultsTitle;

	// "Puesto: #1" / "Eliminado"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResultsRankText;

	// "Tiempo: 12.3s"  — empty if player was eliminated
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResultsTimeText;

	// "Volviendo al lobby en: 8"  — updated every 0.1 s
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResultsCountdown;

	// "Scroll para cambiar de jugador"  — shown only while spectating
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SpectatorHint;

	// Optional Blueprint hook — called when the flow state changes.
	// Override in BP if you need extra visual effects; all logic is already in C++.
	UFUNCTION(BlueprintImplementableEvent, Category = "Flow")
	void OnFlowStateChanged(ETNMatchFlowState NewState);

	/**
	 * Pinta una línea nueva en el feed de quick chat.
	 * La implementación C++ escribe automáticamente en ChatHistoryBox.
	 * Blueprint puede sobreescribir para añadir efectos extra; si lo hace,
	 * llamar a Super para que el C++ también se ejecute.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "QuickChat")
	void OnQuickChatEntryReceived(int32 Sequence, const FText& SenderName, const FText& MessageText, UTexture2D* Icon, float ServerTimeSeconds);

	/**
	 * VerticalBox donde se añaden los mensajes de quick chat.
	 * Añadir en el Designer del Blueprint con el nombre exacto "ChatHistoryBox".
	 * Sin nodos en Event Graph necesarios: la implementación C++ lo rellena.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> ChatHistoryBox;

	/** Máximo de líneas visibles en el feed de chat (las más antiguas se eliminan). */
	UPROPERTY(EditDefaultsOnly, Category = "QuickChat")
	int32 MaxChatLines = 8;

	/** Color del texto de los mensajes de chat. */
	UPROPERTY(EditDefaultsOnly, Category = "QuickChat")
	FLinearColor ChatTextColor = FLinearColor::White;

	/** Segundos de inactividad antes de que el feed de chat empiece a desaparecer. */
	UPROPERTY(EditDefaultsOnly, Category = "QuickChat")
	float ChatInactivitySeconds = 3.f;

	/** Duración del fade-out del feed de chat (segundos). */
	UPROPERTY(EditDefaultsOnly, Category = "QuickChat")
	float ChatFadeOutSeconds = 0.5f;

private:
	void EnsureRuntimeWidgets();
	void RefreshTexts();
	void BindQuickChat(ATN_CoopGameState* GameState);
	void UnbindQuickChat();
	void ReplayQuickChatHistory(const ATN_CoopGameState* GameState);

	UFUNCTION()
	void HandleQuickChatReceived(const FTN_QuickChatEntry& Entry);

	// ── Status strip helpers ───────────────────────────────────────────────────
	FText BuildPrimaryText(const ATN_CoopGameState* GameState) const;
	FText BuildSecondaryText(const ATN_CoopGameState* GameState) const;
	bool  ShouldBeVisible(ETNMatchFlowState State) const;

	// ── Results panel helpers ──────────────────────────────────────────────────
	void HandleFlowStateChange(ETNMatchFlowState NewState, const ATN_CoopGameState* GameState);
	void ShowResultsPanel(const ATN_CoopGameState* GameState);
	void HideResultsPanel();
	void RefreshResultsCountdown(const ATN_CoopGameState* GameState);
	FText BuildRankTitle(int32 FinishRank, bool bEliminated) const;

	void StartChatFade();

	// ── State tracking ─────────────────────────────────────────────────────────
	float RefreshAccumulator  = 0.f;
	float RefreshInterval     = 0.1f;

	ETNMatchFlowState LastKnownFlowState = ETNMatchFlowState::WaitingForPlayers;
	bool bFlowStateInitialized = false;
	bool bResultsVisible       = false;
	int32 LastQuickChatSequenceSeen = 0;
	bool bQuickChatHistoryReplayed = false;

	UPROPERTY(Transient)
	TObjectPtr<ATN_CoopGameState> BoundQuickChatGameState;

	// ── Chat fade state ────────────────────────────────────────────────────────
	FTimerHandle ChatFadeTimer;
	bool  bChatFadingOut     = false;
	float ChatCurrentOpacity = 1.f;

	// ── OnMatchFlowStateChanged binding — backup channel besides polling ──────
	// Binding allows immediate response when state changes, covering the case
	// where the widget is recreated (e.g. after ClientRestart on revive) and
	// the polling misses the transition because the timer hasn't fired yet.
	UFUNCTION()
	void OnMatchFlowStateChangedHandler(ETNMatchFlowState NewState);

	UPROPERTY(Transient)
	TObjectPtr<ATN_CoopGameState> BoundFlowGameState;
};
