#include "UI/HUD/TN_CoopFlowHUDWidget.h"
#include "Core/TN_CoopGameState.h"
#include "Core/TN_CoopPlayerState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "GameFramework/PlayerController.h"

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UTN_CoopFlowHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureRuntimeWidgets();

	// Make sure ResultsOverlay starts hidden even if the designer forgot
	if (ResultsOverlay)
	{
		ResultsOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}

	RefreshTexts();
}

void UTN_CoopFlowHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator < RefreshInterval)
	{
		return;
	}

	RefreshAccumulator = 0.f;
	RefreshTexts();
}

// ─────────────────────────────────────────────────────────────────────────────
// C++ fallback runtime widget creation (used when widget has no BP designer)
// ─────────────────────────────────────────────────────────────────────────────

void UTN_CoopFlowHUDWidget::EnsureRuntimeWidgets()
{
	if (!WidgetTree)
	{
		return;
	}

	if (!RootContainer)
	{
		RootContainer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootContainer"));
		if (RootContainer && !WidgetTree->RootWidget)
		{
			WidgetTree->RootWidget = RootContainer;
		}
	}

	if (!PrimaryText && RootContainer)
	{
		PrimaryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PrimaryText"));
		if (PrimaryText)
		{
			PrimaryText->SetAutoWrapText(true);
			PrimaryText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			if (UVerticalBoxSlot* VBoxSlot = RootContainer->AddChildToVerticalBox(PrimaryText))
			{
				VBoxSlot->SetPadding(FMargin(16.f, 24.f, 16.f, 4.f));
			}
		}
	}

	if (!SecondaryText && RootContainer)
	{
		SecondaryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SecondaryText"));
		if (SecondaryText)
		{
			SecondaryText->SetAutoWrapText(true);
			SecondaryText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f, 1.f)));
			if (UVerticalBoxSlot* VBoxSlot = RootContainer->AddChildToVerticalBox(SecondaryText))
			{
				VBoxSlot->SetPadding(FMargin(16.f, 0.f, 16.f, 0.f));
			}
		}
	}
	// Note: ResultsOverlay and its children are designer-only; no C++ fallback needed.
}

// ─────────────────────────────────────────────────────────────────────────────
// Main refresh (called every 0.1 s)
// ─────────────────────────────────────────────────────────────────────────────

void UTN_CoopFlowHUDWidget::RefreshTexts()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const ATN_CoopGameState* GameState = PC->GetWorld()
		? PC->GetWorld()->GetGameState<ATN_CoopGameState>()
		: nullptr;

	if (!GameState)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// ── Detect state change ──────────────────────────────────────────────────
	if (!bFlowStateInitialized || GameState->MatchFlowState != LastKnownFlowState)
	{
		LastKnownFlowState    = GameState->MatchFlowState;
		bFlowStateInitialized = true;
		HandleFlowStateChange(LastKnownFlowState, GameState);
		OnFlowStateChanged(LastKnownFlowState); // optional BP hook
	}

	// ── Update status-strip texts ────────────────────────────────────────────
	const bool bShow = ShouldBeVisible(GameState->MatchFlowState);
	SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (bShow)
	{
		if (PrimaryText)   { PrimaryText->SetText(BuildPrimaryText(GameState)); }
		if (SecondaryText) { SecondaryText->SetText(BuildSecondaryText(GameState)); }
	}

	// ── Update results countdown every refresh tick ──────────────────────────
	if (bResultsVisible)
	{
		RefreshResultsCountdown(GameState);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Results panel — C++ logic
// ─────────────────────────────────────────────────────────────────────────────

void UTN_CoopFlowHUDWidget::HandleFlowStateChange(ETNMatchFlowState NewState, const ATN_CoopGameState* GameState)
{
	if (NewState == ETNMatchFlowState::Results)
	{
		ShowResultsPanel(GameState);
	}
	else if (bResultsVisible)
	{
		HideResultsPanel();
	}
}

void UTN_CoopFlowHUDWidget::ShowResultsPanel(const ATN_CoopGameState* GameState)
{
	bResultsVisible = true;

	if (ResultsOverlay)
	{
		ResultsOverlay->SetVisibility(ESlateVisibility::Visible);
	}

	// ── Gather local player stats ────────────────────────────────────────────
	const ATN_CoopPlayerState* TNPS = nullptr;
	if (const APlayerController* PC = GetOwningPlayer())
	{
		TNPS = PC->GetPlayerState<ATN_CoopPlayerState>();
	}

	const int32  Rank       = TNPS ? TNPS->FinishRank       : 0;
	const float  Time       = TNPS ? TNPS->FinishTimeSeconds : -1.f;
	const bool   bFinished  = TNPS && TNPS->bHasFinishedRun && Rank > 0;

	// ── Title ────────────────────────────────────────────────────────────────
	if (ResultsTitle)
	{
		ResultsTitle->SetText(BuildRankTitle(Rank));
	}

	// ── Rank line ────────────────────────────────────────────────────────────
	if (ResultsRankText)
	{
		ResultsRankText->SetText(bFinished
			? FText::FromString(FString::Printf(TEXT("Puesto: #%d"), Rank))
			: FText::FromString(TEXT("Eliminado")));
	}

	// ── Time line ────────────────────────────────────────────────────────────
	if (ResultsTimeText)
	{
		ResultsTimeText->SetText(Time > 0.f
			? FText::FromString(FString::Printf(TEXT("Tiempo: %.1fs"), Time))
			: FText::GetEmpty());
	}

	// ── Spectator hint ───────────────────────────────────────────────────────
	if (SpectatorHint)
	{
		SpectatorHint->SetText(!bFinished
			? FText::FromString(TEXT("Scroll para cambiar de jugador"))
			: FText::GetEmpty());
	}

	// ── Initial countdown ────────────────────────────────────────────────────
	RefreshResultsCountdown(GameState);
}

void UTN_CoopFlowHUDWidget::HideResultsPanel()
{
	bResultsVisible = false;

	if (ResultsOverlay)
	{
		ResultsOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UTN_CoopFlowHUDWidget::RefreshResultsCountdown(const ATN_CoopGameState* GameState)
{
	if (!ResultsCountdown || !GameState)
	{
		return;
	}

	ResultsCountdown->SetText(
		FText::FromString(FString::Printf(TEXT("Volviendo al lobby en: %d"), GameState->CountdownValue)));
}

FText UTN_CoopFlowHUDWidget::BuildRankTitle(int32 FinishRank) const
{
	switch (FinishRank)
	{
	case 1:  return FText::FromString(TEXT("¡PRIMER LUGAR!"));
	case 2:  return FText::FromString(TEXT("¡SEGUNDO LUGAR!"));
	case 3:  return FText::FromString(TEXT("¡TERCER LUGAR!"));
	default:
		return FinishRank > 0
			? FText::FromString(FString::Printf(TEXT("PUESTO #%d"), FinishRank))
			: FText::FromString(TEXT("¡ELIMINADO!"));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Status-strip text builders
// ─────────────────────────────────────────────────────────────────────────────

FText UTN_CoopFlowHUDWidget::BuildPrimaryText(const ATN_CoopGameState* GameState) const
{
	switch (GameState->MatchFlowState)
	{
	case ETNMatchFlowState::WaitingForPlayers:
		return FText::FromString(FString::Printf(
			TEXT("Sala: %d/%d | Zona: %d/%d"),
			GameState->ConnectedPlayers, GameState->ExpectedPlayers,
			GameState->PlayersInStartZone, GameState->ConnectedPlayers));
	case ETNMatchFlowState::Countdown:
		return FText::FromString(FString::Printf(TEXT("Todos listos! Empieza en: %d"), GameState->CountdownValue));
	case ETNMatchFlowState::Cinematic:
		return FText::FromString(TEXT("Preparando..."));
	case ETNMatchFlowState::InProgress:
		return FText::FromString(FString::Printf(TEXT("Carrera en curso. Meta: %d/%d"),
			GameState->FinishedPlayers, GameState->ExpectedPlayers));
	case ETNMatchFlowState::Results:
		return FText::FromString(FString::Printf(TEXT("Volviendo al lobby en: %d"), GameState->CountdownValue));
	default:
		return FText::GetEmpty();
	}
}

FText UTN_CoopFlowHUDWidget::BuildSecondaryText(const ATN_CoopGameState* GameState) const
{
	switch (GameState->MatchFlowState)
	{
	case ETNMatchFlowState::WaitingForPlayers:
		return FText::FromString(TEXT("El contador arranca cuando todos los jugadores conectados esten en la zona."));
	case ETNMatchFlowState::Countdown:
		return FText::FromString(FString::Printf(
			TEXT("Conectados: %d/%d | Zona: %d/%d"),
			GameState->ConnectedPlayers, GameState->ExpectedPlayers,
			GameState->PlayersInStartZone, GameState->ConnectedPlayers));
	case ETNMatchFlowState::Cinematic:
		return FText::FromString(TEXT("Mantente preparado para el viaje al mapa de carrera."));
	case ETNMatchFlowState::InProgress:
		if (const APlayerController* PC = GetOwningPlayer())
		{
			if (const ATN_CoopPlayerState* TNPS = PC->GetPlayerState<ATN_CoopPlayerState>())
			{
				if (TNPS->DeathZoneTimeRemaining >= 0.f)
				{
					return FText::FromString(FString::Printf(
						TEXT("Peligro: sal de la zona de muerte (%.1fs)"), TNPS->DeathZoneTimeRemaining));
				}
			}
		}
		return FText::FromString(TEXT("Cruza la meta para pasar a espectador."));
	case ETNMatchFlowState::Results:
		return FText::GetEmpty(); // Results panel covers this state
	default:
		return FText::GetEmpty();
	}
}

bool UTN_CoopFlowHUDWidget::ShouldBeVisible(ETNMatchFlowState State) const
{
	return State == ETNMatchFlowState::WaitingForPlayers
		|| State == ETNMatchFlowState::Countdown
		|| State == ETNMatchFlowState::Cinematic
		|| State == ETNMatchFlowState::InProgress
		|| State == ETNMatchFlowState::Results;
}
