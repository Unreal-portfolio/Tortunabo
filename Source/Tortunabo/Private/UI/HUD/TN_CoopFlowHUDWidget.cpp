#include "UI/HUD/TN_CoopFlowHUDWidget.h"
#include "Core/TN_CoopGameState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"

void UTN_CoopFlowHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureRuntimeWidgets();
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
}

void UTN_CoopFlowHUDWidget::RefreshTexts()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const ATN_CoopGameState* GameState = PC->GetWorld() ? PC->GetWorld()->GetGameState<ATN_CoopGameState>() : nullptr;
	if (!GameState)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const bool bShow = ShouldBeVisible(GameState->MatchFlowState);
	SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bShow)
	{
		return;
	}

	if (PrimaryText)
	{
		PrimaryText->SetText(BuildPrimaryText(GameState));
	}

	if (SecondaryText)
	{
		SecondaryText->SetText(BuildSecondaryText(GameState));
	}
}

FText UTN_CoopFlowHUDWidget::BuildPrimaryText(const ATN_CoopGameState* GameState) const
{
	switch (GameState->MatchFlowState)
	{
	case ETNMatchFlowState::WaitingForPlayers:
		return FText::FromString(FString::Printf(TEXT("Esperando jugadores listos: %d/%d"), GameState->ReadyPlayers, GameState->ExpectedPlayers));
	case ETNMatchFlowState::Countdown:
		return FText::FromString(FString::Printf(TEXT("La partida empieza en: %d"), GameState->CountdownValue));
	case ETNMatchFlowState::Cinematic:
		return FText::FromString(TEXT("Preparando cinematicas..."));
	case ETNMatchFlowState::InProgress:
		return FText::FromString(FString::Printf(TEXT("Carrera en curso. Meta: %d/%d"), GameState->FinishedPlayers, GameState->ExpectedPlayers));
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
		return FText::FromString(TEXT("Entra en la zona ready para iniciar el countdown."));
	case ETNMatchFlowState::Countdown:
		return FText::FromString(TEXT("Si alguien sale de la zona ready, se cancela."));
	case ETNMatchFlowState::Cinematic:
		return FText::FromString(TEXT("Mantente preparado para el viaje al mapa de carrera."));
	case ETNMatchFlowState::InProgress:
		return FText::FromString(TEXT("Cruza la meta para pasar a espectador."));
	case ETNMatchFlowState::Results:
		return FText::FromString(TEXT("Resultados cerrados. Espera el viaje automatico."));
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



