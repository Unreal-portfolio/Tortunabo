#include "UI/HUD/TN_CoopFlowHUDWidget.h"
#include "Core/TN_CoopGameState.h"
#include "Core/TN_CoopPlayerState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Components/Widget.h"
#include "GameFramework/PlayerController.h"
#include "Player/MP_GamePlayerController.h"

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

void UTN_CoopFlowHUDWidget::NativeDestruct()
{
	UnbindQuickChat();
	Super::NativeDestruct();
}

void UTN_CoopFlowHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// ── Chat fade-out ─────────────────────────────────────────────────────────
	if (bChatFadingOut && ChatHistoryBox)
	{
		ChatCurrentOpacity -= InDeltaTime / FMath::Max(ChatFadeOutSeconds, KINDA_SMALL_NUMBER);
		if (ChatCurrentOpacity <= 0.f)
		{
			ChatCurrentOpacity = 0.f;
			bChatFadingOut = false;
			ChatHistoryBox->SetVisibility(ESlateVisibility::Collapsed);
			ChatHistoryBox->ClearChildren();
		}
		else
		{
			ChatHistoryBox->SetRenderOpacity(ChatCurrentOpacity);
		}
	}

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

	ATN_CoopGameState* GameState = PC->GetWorld()
		? PC->GetWorld()->GetGameState<ATN_CoopGameState>()
		: nullptr;

	if (!GameState)
	{
		UnbindQuickChat();
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	BindQuickChat(GameState);

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

void UTN_CoopFlowHUDWidget::BindQuickChat(ATN_CoopGameState* GameState)
{
	if (!GameState)
	{
		UnbindQuickChat();
		return;
	}

	if (BoundQuickChatGameState == GameState)
	{
		if (!bQuickChatHistoryReplayed)
		{
			ReplayQuickChatHistory(GameState);
		}
		return;
	}

	UnbindQuickChat();
	BoundQuickChatGameState = GameState;
	GameState->OnQuickChatReceived.AddUniqueDynamic(this, &UTN_CoopFlowHUDWidget::HandleQuickChatReceived);
	ReplayQuickChatHistory(GameState);
}

void UTN_CoopFlowHUDWidget::UnbindQuickChat()
{
	if (BoundQuickChatGameState)
	{
		BoundQuickChatGameState->OnQuickChatReceived.RemoveDynamic(this, &UTN_CoopFlowHUDWidget::HandleQuickChatReceived);
		BoundQuickChatGameState = nullptr;
	}

	bQuickChatHistoryReplayed = false;
	LastQuickChatSequenceSeen = 0;
}

void UTN_CoopFlowHUDWidget::ReplayQuickChatHistory(const ATN_CoopGameState* GameState)
{
	if (!GameState)
	{
		return;
	}

	for (const FTN_QuickChatEntry& Entry : GameState->QuickChatHistory)
	{
		HandleQuickChatReceived(Entry);
	}

	bQuickChatHistoryReplayed = true;
}

void UTN_CoopFlowHUDWidget::HandleQuickChatReceived(const FTN_QuickChatEntry& Entry)
{
	if (Entry.Sequence <= LastQuickChatSequenceSeen)
	{
		return;
	}

	LastQuickChatSequenceSeen = Entry.Sequence;

	FText SenderName = FText::GetEmpty();
	FText MessageText = FText::GetEmpty();
	UTexture2D* Icon = nullptr;

	bool bResolved = false;
	if (const AMP_GamePlayerController* PC = Cast<AMP_GamePlayerController>(GetOwningPlayer()))
	{
		bResolved = PC->ResolveQuickChatDisplayData(Entry, SenderName, MessageText, Icon);
	}

	if (!bResolved)
	{
		if (BoundQuickChatGameState)
		{
			SenderName = BoundQuickChatGameState->ResolveQuickChatSenderName(Entry.SenderPlayerId);
		}

		if (MessageText.IsEmpty())
		{
			MessageText = FText::FromString(FString::Printf(TEXT("Mensaje #%d"), static_cast<int32>(Entry.MessageID)));
		}
	}

	OnQuickChatEntryReceived(Entry.Sequence, SenderName, MessageText, Icon, Entry.ServerTime);
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

	const int32  Rank          = TNPS ? TNPS->FinishRank       : 0;
	const float  Time          = TNPS ? TNPS->FinishTimeSeconds : -1.f;
	const bool   bEliminated   = TNPS && TNPS->bIsEliminated;
	const bool   bFinishedNorm = TNPS && TNPS->bHasFinishedRun && Rank > 0 && !bEliminated;

	// ── Title ────────────────────────────────────────────────────────────────
	if (ResultsTitle)
	{
		ResultsTitle->SetText(BuildRankTitle(Rank, bEliminated));
	}

	// ── Rank line ────────────────────────────────────────────────────────────
	if (ResultsRankText)
	{
		if (bEliminated)
		{
			ResultsRankText->SetText(FText::FromString(TEXT("Eliminado")));
		}
		else if (bFinishedNorm)
		{
			ResultsRankText->SetText(FText::FromString(FString::Printf(TEXT("Puesto: #%d"), Rank)));
		}
		else
		{
			ResultsRankText->SetText(FText::GetEmpty());
		}
	}

	// ── Time line ────────────────────────────────────────────────────────────
	if (ResultsTimeText)
	{
		ResultsTimeText->SetText((bFinishedNorm && Time > 0.f)
			? FText::FromString(FString::Printf(TEXT("Tiempo: %.1fs"), Time))
			: FText::GetEmpty());
	}

	// ── Spectator hint ───────────────────────────────────────────────────────
	if (SpectatorHint)
	{
		SpectatorHint->SetText(bEliminated
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

FText UTN_CoopFlowHUDWidget::BuildRankTitle(int32 FinishRank, bool bEliminated) const
{
	if (bEliminated)
	{
		return FText::FromString(TEXT("¡Eliminado!"));
	}

	switch (FinishRank)
	{
	case 1:  return FText::FromString(TEXT("¡Primero!"));
	case 2:  return FText::FromString(TEXT("Segundo"));
	case 3:  return FText::FromString(TEXT("Tercero"));
	default:
		return FinishRank > 0
			? FText::FromString(FString::Printf(TEXT("Puesto #%d"), FinishRank))
			: FText::FromString(TEXT("¡Eliminado!"));
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

// ─────────────────────────────────────────────────────────────────────────────
// Quick Chat feed — C++ implementation
// ─────────────────────────────────────────────────────────────────────────────

void UTN_CoopFlowHUDWidget::OnQuickChatEntryReceived_Implementation(
	int32 Sequence, const FText& SenderName, const FText& MessageText,
	UTexture2D* Icon, float ServerTimeSeconds)
{
	if (!ChatHistoryBox)
	{
		return;
	}

	// Si el feed ya estaba oculto o casi invisible, limpiar historial para
	// que solo aparezca el nuevo mensaje (no los mensajes viejos de golpe).
	if (ChatCurrentOpacity < 0.1f || ChatHistoryBox->GetVisibility() == ESlateVisibility::Collapsed)
	{
		ChatHistoryBox->ClearChildren();
	}

	// ── Build entry row: [Icon?] [Sender: Message] ────────────────────────────
	UHorizontalBox* Row = NewObject<UHorizontalBox>(this);

	if (Icon)
	{
		UImage* IconWidget = NewObject<UImage>(this);
		IconWidget->SetBrushFromTexture(Icon, false);
		IconWidget->SetBrushSize(FVector2D(20.f, 20.f));
		if (UHorizontalBoxSlot* HSlot = Row->AddChildToHorizontalBox(IconWidget))
		{
			HSlot->SetVerticalAlignment(VAlign_Center);
			HSlot->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));
		}
	}

	const FString FullText = FString::Printf(TEXT("%s: %s"), *SenderName.ToString(), *MessageText.ToString());
	UTextBlock* TextWidget = NewObject<UTextBlock>(this);
	TextWidget->SetText(FText::FromString(FullText));
	TextWidget->SetColorAndOpacity(FSlateColor(FLinearColor(ChatTextColor)));
	TextWidget->SetAutoWrapText(true);
	if (UHorizontalBoxSlot* HSlot = Row->AddChildToHorizontalBox(TextWidget))
	{
		HSlot->SetVerticalAlignment(VAlign_Center);
		HSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// ── Add to feed and enforce max line count ────────────────────────────────
	if (UVerticalBoxSlot* VSlot = ChatHistoryBox->AddChildToVerticalBox(Row))
	{
		VSlot->SetPadding(FMargin(0.f, 2.f));
	}

	while (ChatHistoryBox->GetChildrenCount() > MaxChatLines)
	{
		ChatHistoryBox->RemoveChildAt(0);
	}

	// Restaurar visibilidad y opacidad; resetear timer de inactividad.
	ChatHistoryBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	ChatHistoryBox->SetRenderOpacity(1.f);
	bChatFadingOut     = false;
	ChatCurrentOpacity = 1.f;

	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->GetWorldTimerManager().SetTimer(
			ChatFadeTimer, this, &UTN_CoopFlowHUDWidget::StartChatFade,
			ChatInactivitySeconds, false);
	}
}

void UTN_CoopFlowHUDWidget::StartChatFade()
{
	bChatFadingOut = true;
}
