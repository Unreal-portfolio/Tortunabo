#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/TN_MatchFlowTypes.h"
#include "TN_CoopFlowHUDWidget.generated.h"

class UTextBlock;
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

private:
	void EnsureRuntimeWidgets();
	void RefreshTexts();

	// ── Status strip helpers ───────────────────────────────────────────────────
	FText BuildPrimaryText(const ATN_CoopGameState* GameState) const;
	FText BuildSecondaryText(const ATN_CoopGameState* GameState) const;
	bool  ShouldBeVisible(ETNMatchFlowState State) const;

	// ── Results panel helpers ──────────────────────────────────────────────────
	void HandleFlowStateChange(ETNMatchFlowState NewState, const ATN_CoopGameState* GameState);
	void ShowResultsPanel(const ATN_CoopGameState* GameState);
	void HideResultsPanel();
	void RefreshResultsCountdown(const ATN_CoopGameState* GameState);
	FText BuildRankTitle(int32 FinishRank) const;

	// ── State tracking ─────────────────────────────────────────────────────────
	float RefreshAccumulator  = 0.f;
	float RefreshInterval     = 0.1f;

	ETNMatchFlowState LastKnownFlowState = ETNMatchFlowState::WaitingForPlayers;
	bool bFlowStateInitialized = false;
	bool bResultsVisible       = false;
};
