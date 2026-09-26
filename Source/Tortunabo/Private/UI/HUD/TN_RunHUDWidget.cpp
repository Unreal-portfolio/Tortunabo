#include "UI/HUD/TN_RunHUDWidget.h"
#include "TN_HUDStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/TN_CoopPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Player/TortugaCharacter.h"

namespace
{
	using namespace TNHUDStyle;

	template <typename T>
	T* Make(UWidgetTree* Tree, const TCHAR* Name = nullptr)
	{
		return Tree->ConstructWidget<T>(T::StaticClass(), Name ? FName(Name) : NAME_None);
	}

	UTextBlock* MakeText(UWidgetTree* Tree, const TCHAR* Name, const FText& Content, FName Weight, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* T = Make<UTextBlock>(Tree, Name);
		T->SetText(Content);
		StyleText(T, Weight, Size, Color);
		return T;
	}

	UBorder* MakePanel(UWidgetTree* Tree, UWidget* Content, const FLinearColor& Fill, float Radius, const FMargin& Padding,
		const FLinearColor& Outline = Edge, float OutlineWidth = 1.5f)
	{
		UBorder* B = Make<UBorder>(Tree);
		StylePanel(B, Fill, Radius, Padding, Outline, OutlineWidth);
		B->SetHorizontalAlignment(HAlign_Fill);
		B->SetVerticalAlignment(VAlign_Fill);
		if (Content) { B->SetContent(Content); }
		return B;
	}

	USizeBox* MakeSize(UWidgetTree* Tree, UWidget* Content, float W, float H)
	{
		USizeBox* S = Make<USizeBox>(Tree);
		if (W > 0.f) { S->SetWidthOverride(W); }
		if (H > 0.f) { S->SetHeightOverride(H); }
		if (Content) { S->SetContent(Content); }
		return S;
	}

	/** Los hijos de un Overlay ocupan todo su hueco (por defecto van arriba a la izquierda con su tamaño propio). */
	void FillOverlay(UOverlaySlot* Slot)
	{
		if (!Slot) { return; }
		Slot->SetHorizontalAlignment(HAlign_Fill);
		Slot->SetVerticalAlignment(VAlign_Fill);
	}

	UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* W, const FVector2D& Anchor, const FVector2D& Offset)
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(W);
		Slot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		Slot->SetAlignment(Anchor);
		Slot->SetPosition(Offset);
		Slot->SetAutoSize(true);
		return Slot;
	}

	/** Color de la estamina: turquesa llena, ámbar a media, coral casi vacía. */
	FLinearColor StaminaColor(float Ratio)
	{
		const FLinearColor Amber(1.f, 0.72f, 0.2f, 1.f);
		if (Ratio > 0.5f) { return FMath::Lerp(Amber, Accent, (Ratio - 0.5f) * 2.f); }
		return FMath::Lerp(Coral, Amber, Ratio * 2.f);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// HUD de la tortuga
// ─────────────────────────────────────────────────────────────────────────────

void UTN_RunHUDWidget::NativeOnInitialized()
{
	BuildTree();
	Super::NativeOnInitialized();
}

void UTN_RunHUDWidget::BuildTree()
{
	if (!WidgetTree || Canvas) { return; }
	UWidgetTree* Tree = WidgetTree;
	Canvas = Make<UCanvasPanel>(Tree, TEXT("RunHUDCanvas"));
	Tree->RootWidget = Canvas;

	// ── Estamina (abajo a la izquierda): caparazón, barra sin número y aviso de agotada ──
	{
		UHorizontalBox* Row = Make<UHorizontalBox>(Tree);
		UBorder* Shell = MakePanel(Tree, MakePanel(Tree, nullptr, FLinearColor(0.02f, 0.3f, 0.26f, 1.f), 12.f, FMargin(0.f), FLinearColor(0.f, 0.f, 0.f, 0.f), 0.f),
			Accent * FLinearColor(1.f, 1.f, 1.f, 0.9f), 22.f, FMargin(9.f), Sand, 2.5f);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(MakeSize(Tree, Shell, 44.f, 44.f))) { S->SetVerticalAlignment(VAlign_Center); }

		UVerticalBox* Col = Make<UVerticalBox>(Tree);
		Col->AddChildToVerticalBox(MakeText(Tree, nullptr, NSLOCTEXT("TNHUD", "Stamina", "ENERGÍA"), TEXT("Bold"), 12, TextDim));
		UOverlay* Bars = Make<UOverlay>(Tree);
		StaminaBar = Make<UProgressBar>(Tree, TEXT("StaminaBar"));
		StaminaBar->SetWidgetStyle(Bar(9.f));
		StaminaBar->SetFillColorAndOpacity(Accent);
		StaminaBar->SetPercent(1.f);
		FillOverlay(Bars->AddChildToOverlay(StaminaBar));
		WeightPenaltyBar = Make<UProgressBar>(Tree, TEXT("WeightPenaltyBar"));
		FProgressBarStyle Heavy = Bar(9.f);
		Heavy.SetBackgroundImage(Rounded(FLinearColor::Transparent, 9.f));
		WeightPenaltyBar->SetWidgetStyle(Heavy);
		WeightPenaltyBar->SetBarFillType(EProgressBarFillType::RightToLeft);
		WeightPenaltyBar->SetFillColorAndOpacity(FLinearColor(0.35f, 0.2f, 0.1f, 0.85f));
		WeightPenaltyBar->SetPercent(0.f);
		FillOverlay(Bars->AddChildToOverlay(WeightPenaltyBar));
		if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(MakeSize(Tree, Bars, 300.f, 18.f))) { S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f)); }
		UTextBlock* Tired = MakeText(Tree, nullptr, NSLOCTEXT("TNHUD", "Exhausted", "¡Sin aliento!"), TEXT("Bold"), 13, Coral);
		ExhaustedRoot = Tired;
		ExhaustedRoot->SetVisibility(ESlateVisibility::Hidden);
		if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Tired)) { S->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f)); }
		// El número de la estamina no se muestra (la barra basta); el texto existe para la clase base.
		StaminaText = MakeText(Tree, TEXT("StaminaText"), FText::GetEmpty(), TEXT("Regular"), 12, TextDim);
		StaminaText->SetVisibility(ESlateVisibility::Collapsed);
		Col->AddChildToVerticalBox(StaminaText);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Col)) { S->SetPadding(FMargin(12.f, 0.f, 4.f, 0.f)); S->SetVerticalAlignment(VAlign_Center); }
		Place(Canvas, MakePanel(Tree, Row, Panel, 20.f, FMargin(12.f, 10.f, 16.f, 10.f)), FVector2D(0.f, 1.f), FVector2D(32.f, -32.f));
	}

	// ── Inventario (abajo en el centro): equipado grande con marco turquesa y guardado al lado ──
	{
		UHorizontalBox* Row = Make<UHorizontalBox>(Tree);
		UOverlay* Equipped = Make<UOverlay>(Tree);
		SlotEquippedImage = Make<UImage>(Tree, TEXT("SlotEquippedImage"));
		SlotEquippedImage->SetColorAndOpacity(FLinearColor::Transparent);
		FillOverlay(Equipped->AddChildToOverlay(MakePanel(Tree, SlotEquippedImage, Panel, 18.f, FMargin(14.f))));
		UBorder* Selector = MakePanel(Tree, nullptr, FLinearColor::Transparent, 18.f, FMargin(0.f), Accent, 3.f);
		SlotEquippedSelector = Selector;
		FillOverlay(Equipped->AddChildToOverlay(Selector));
		UVerticalBox* ColA = Make<UVerticalBox>(Tree);
		ColA->AddChildToVerticalBox(MakeSize(Tree, Equipped, 92.f, 92.f));
		if (UVerticalBoxSlot* S = ColA->AddChildToVerticalBox(MakeText(Tree, nullptr, NSLOCTEXT("TNHUD", "InHand", "EN MANO"), TEXT("Bold"), 11, TextDim)))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
		Row->AddChildToHorizontalBox(ColA);

		SlotStoredImage = Make<UImage>(Tree, TEXT("SlotStoredImage"));
		SlotStoredImage->SetColorAndOpacity(FLinearColor::Transparent);
		UVerticalBox* ColB = Make<UVerticalBox>(Tree);
		ColB->AddChildToVerticalBox(MakeSize(Tree, MakePanel(Tree, SlotStoredImage, PanelSoft, 14.f, FMargin(11.f)), 68.f, 68.f));
		if (UVerticalBoxSlot* S = ColB->AddChildToVerticalBox(MakeText(Tree, nullptr, NSLOCTEXT("TNHUD", "Stored", "GUARDADO"), TEXT("Bold"), 11, TextDim)))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(ColB)) { S->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f)); S->SetVerticalAlignment(VAlign_Bottom); }
		Place(Canvas, Row, FVector2D(0.5f, 1.f), FVector2D(0.f, -26.f));
	}

	// ── Puntos (arriba a la derecha): insignia con estrella ──
	{
		UHorizontalBox* Row = Make<UHorizontalBox>(Tree);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(MakeText(Tree, nullptr, FText::FromString(TEXT("★")), TEXT("Bold"), 26, Sand)))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
		ScoreText = MakeText(Tree, TEXT("ScoreText"), FText::AsNumber(0), TEXT("Bold"), 30, Text);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(ScoreText)) { S->SetPadding(FMargin(10.f, 0.f, 4.f, 0.f)); S->SetVerticalAlignment(VAlign_Center); }
		ScoreBadge = MakePanel(Tree, Row, Panel, 24.f, FMargin(18.f, 6.f, 20.f, 6.f), Sand * FLinearColor(1.f, 1.f, 1.f, 0.6f), 2.f);
		ScoreBadge->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Place(Canvas, ScoreBadge, FVector2D(1.f, 0.f), FVector2D(-32.f, 26.f));
	}

	// ── Avisos (en el centro): tormenta, derribado y reanimación ──
	{
		StormText = MakeText(Tree, nullptr, FText::GetEmpty(), TEXT("Bold"), 24, Text);
		StormBanner = MakePanel(Tree, StormText, Coral * FLinearColor(0.85f, 0.85f, 0.85f, 0.88f), 22.f, FMargin(26.f, 10.f), Sand, 2.f);
		StormBanner->SetVisibility(ESlateVisibility::Collapsed);
		Place(Canvas, StormBanner, FVector2D(0.5f, 0.f), FVector2D(0.f, 110.f));

		DownText = MakeText(Tree, nullptr, FText::GetEmpty(), TEXT("Bold"), 22, Text);
		DownText->SetJustification(ETextJustify::Center);
		DownBanner = MakePanel(Tree, DownText, Panel, 20.f, FMargin(24.f, 12.f), Coral, 2.f);
		DownBanner->SetVisibility(ESlateVisibility::Collapsed);
		Place(Canvas, DownBanner, FVector2D(0.5f, 0.5f), FVector2D(0.f, 150.f));

		UVerticalBox* Col = Make<UVerticalBox>(Tree);
		if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(MakeText(Tree, nullptr, NSLOCTEXT("TNHUD", "Reviving", "Reanimando..."), TEXT("Bold"), 16, Text)))
		{
			S->SetHorizontalAlignment(HAlign_Center);
		}
		ReviveBar = Make<UProgressBar>(Tree);
		ReviveBar->SetWidgetStyle(Bar(7.f));
		ReviveBar->SetFillColorAndOpacity(Accent);
		if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(MakeSize(Tree, ReviveBar, 240.f, 14.f))) { S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f)); }
		ReviveBanner = MakePanel(Tree, Col, Panel, 16.f, FMargin(18.f, 10.f));
		ReviveBanner->SetVisibility(ESlateVisibility::Collapsed);
		Place(Canvas, ReviveBanner, FVector2D(0.5f, 0.5f), FVector2D(0.f, 90.f));
	}
}

void UTN_RunHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Time += InDeltaTime;
	TickStamina(InDeltaTime);
	TickScore(InDeltaTime);
	TickAlerts(InDeltaTime);
}

void UTN_RunHUDWidget::TickStamina(float DeltaTime)
{
	if (!StaminaBar) { return; }
	// Color según lo que queda (suavizado) y el aviso de agotada late.
	ShownStamina = FMath::FInterpTo(ShownStamina, StaminaBar->GetPercent(), DeltaTime, 8.f);
	StaminaBar->SetFillColorAndOpacity(StaminaColor(ShownStamina));
	if (ExhaustedRoot && ExhaustedRoot->IsVisible())
	{
		ExhaustedRoot->SetRenderOpacity(0.55f + 0.45f * FMath::Abs(FMath::Sin(Time * 5.f)));
	}
}

void UTN_RunHUDWidget::TickScore(float DeltaTime)
{
	if (!ScoreText || !ScoreBadge) { return; }
	const FString Now = ScoreText->GetText().ToString();
	if (!LastScore.IsEmpty() && Now != LastScore) { ScorePop = 1.f; }
	LastScore = Now;
	ScorePop = FMath::Max(0.f, ScorePop - DeltaTime * 3.f);
	const float Scale = 1.f + 0.28f * FMath::Sin(ScorePop * PI);
	ScoreBadge->SetRenderScale(FVector2D(Scale, Scale));
}

void UTN_RunHUDWidget::TickAlerts(float DeltaTime)
{
	const APlayerController* PC = GetOwningPlayer();
	const ATN_CoopPlayerState* PS = PC ? PC->GetPlayerState<ATN_CoopPlayerState>() : nullptr;

	// Tormenta: cuenta atrás mientras se está dentro.
	const bool bStorm = PS && PS->DeathZoneTimeRemaining >= 0.f && PS->bIsAlive;
	if (StormBanner)
	{
		StormBanner->SetVisibility(bStorm ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bStorm && StormText)
		{
			FNumberFormattingOptions OneDecimal;
			OneDecimal.SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(1);
			StormText->SetText(FText::Format(NSLOCTEXT("TNHUD", "Storm", "¡SAL DE LA TORMENTA!   {0}"), FText::AsNumber(PS->DeathZoneTimeRemaining, &OneDecimal)));
			StormBanner->SetRenderOpacity(0.7f + 0.3f * FMath::Abs(FMath::Sin(Time * 6.f)));
		}
	}

	// Derribado: lo que queda para que te reanimen.
	const bool bDown = PS && PS->bIsDBNO;
	if (DownBanner)
	{
		DownBanner->SetVisibility(bDown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bDown && DownText)
		{
			DownText->SetText(FText::Format(NSLOCTEXT("TNHUD", "Down", "Estás derribada\nUn compañero puede reanimarte:  {0} s"),
				FText::AsNumber(FMath::Max(0, FMath::CeilToInt(PS->DBNOBleedoutTimeRemaining)))));
		}
	}

	// Reanimando a un compañero.
	const ATortugaCharacter* Turtle = PC ? Cast<ATortugaCharacter>(PC->GetPawn()) : nullptr;
	const bool bReviving = Turtle && Turtle->bIsReviving;
	if (ReviveBanner)
	{
		ReviveBanner->SetVisibility(bReviving ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bReviving && ReviveBar) { ReviveBar->SetPercent(FMath::Clamp(Turtle->ReviveProgress, 0.f, 1.f)); }
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Franja de estado, resultados y mensajes
// ─────────────────────────────────────────────────────────────────────────────

void UTN_RunFlowHUDWidget::NativeOnInitialized()
{
	BuildTree();
	Super::NativeOnInitialized();
}

void UTN_RunFlowHUDWidget::BuildTree()
{
	if (!WidgetTree || Canvas) { return; }
	UWidgetTree* Tree = WidgetTree;
	Canvas = Make<UCanvasPanel>(Tree, TEXT("RunFlowCanvas"));
	Tree->RootWidget = Canvas;

	// ── Franja de estado (arriba a la izquierda): filo turquesa y dos líneas ──
	{
		UHorizontalBox* Row = Make<UHorizontalBox>(Tree);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(MakeSize(Tree, MakePanel(Tree, nullptr, Accent, 3.f, FMargin(0.f), FLinearColor::Transparent, 0.f), 5.f, 0.f)))
		{
			S->SetVerticalAlignment(VAlign_Fill);
		}
		RootContainer = Make<UVerticalBox>(Tree, TEXT("RootContainer"));
		PrimaryText = MakeText(Tree, TEXT("PrimaryText"), FText::GetEmpty(), TEXT("Bold"), 21, Text);
		PrimaryText->SetAutoWrapText(true);
		RootContainer->AddChildToVerticalBox(PrimaryText);
		SecondaryText = MakeText(Tree, TEXT("SecondaryText"), FText::GetEmpty(), TEXT("Regular"), 15, TextDim);
		SecondaryText->SetAutoWrapText(true);
		if (UVerticalBoxSlot* S = RootContainer->AddChildToVerticalBox(SecondaryText)) { S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f)); }
		USizeBox* Limit = MakeSize(Tree, RootContainer, 0.f, 0.f);
		Limit->SetMaxDesiredWidth(520.f);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Limit)) { S->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f)); }
		Place(Canvas, MakePanel(Tree, Row, Panel, 16.f, FMargin(12.f, 10.f, 20.f, 12.f)), FVector2D(0.f, 0.f), FVector2D(28.f, 24.f));
	}

	// ── Mensajes rápidos (izquierda, a media altura) ──
	{
		ChatHistoryBox = Make<UVerticalBox>(Tree, TEXT("ChatHistoryBox"));
		Place(Canvas, MakePanel(Tree, ChatHistoryBox, PanelSoft, 14.f, FMargin(12.f, 8.f), FLinearColor::Transparent, 0.f), FVector2D(0.f, 0.5f), FVector2D(28.f, 0.f));
	}

	// ── Aviso de espectador (abajo, sobre el inventario) ──
	{
		SpectatorHint = MakeText(Tree, TEXT("SpectatorHint"), FText::GetEmpty(), TEXT("Bold"), 15, Sand);
		SpectatorHint->SetVisibility(ESlateVisibility::Collapsed);
		Place(Canvas, SpectatorHint, FVector2D(0.5f, 1.f), FVector2D(0.f, -150.f));
	}

	// ── Resultados (pantalla atenuada y tarjeta central con la clasificación) ──
	{
		UVerticalBox* Card = Make<UVerticalBox>(Tree);
		ResultsTitle = MakeText(Tree, TEXT("ResultsTitle"), FText::GetEmpty(), TEXT("Bold"), 44, Sand);
		ResultsTitle->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = Card->AddChildToVerticalBox(ResultsTitle)) { S->SetHorizontalAlignment(HAlign_Center); }
		ResultsRankText = MakeText(Tree, TEXT("ResultsRankText"), FText::GetEmpty(), TEXT("Bold"), 24, Text);
		if (UVerticalBoxSlot* S = Card->AddChildToVerticalBox(ResultsRankText)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f)); }
		ResultsTimeText = MakeText(Tree, TEXT("ResultsTimeText"), FText::GetEmpty(), TEXT("Regular"), 20, TextDim);
		if (UVerticalBoxSlot* S = Card->AddChildToVerticalBox(ResultsTimeText)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 2.f, 0.f, 14.f)); }

		// Clasificación: cuatro filas (puesto, nombre, tiempo, puntos), alternando el fondo.
		TObjectPtr<UTextBlock>* Cells[4][4] = {
			{ &Row1RankText, &Row1NameText, &Row1TimeText, &Row1ScoreText },
			{ &Row2RankText, &Row2NameText, &Row2TimeText, &Row2ScoreText },
			{ &Row3RankText, &Row3NameText, &Row3TimeText, &Row3ScoreText },
			{ &Row4RankText, &Row4NameText, &Row4TimeText, &Row4ScoreText } };
		const float Widths[4] = { 60.f, 280.f, 130.f, 100.f };
		for (int32 r = 0; r < 4; ++r)
		{
			UHorizontalBox* Line = Make<UHorizontalBox>(Tree);
			for (int32 c = 0; c < 4; ++c)
			{
				UTextBlock* Cell = MakeText(Tree, nullptr, FText::GetEmpty(), c == 1 ? TEXT("Bold") : TEXT("Regular"), 19, c == 0 ? Sand : Text);
				Cell->SetJustification(c >= 2 ? ETextJustify::Right : ETextJustify::Left);
				*Cells[r][c] = Cell;
				Line->AddChildToHorizontalBox(MakeSize(Tree, Cell, Widths[c], 0.f));
			}
			UBorder* Stripe = MakePanel(Tree, Line, (r % 2) == 0 ? FLinearColor(1.f, 1.f, 1.f, 0.06f) : FLinearColor::Transparent, 10.f, FMargin(14.f, 6.f),
				FLinearColor::Transparent, 0.f);
			if (UVerticalBoxSlot* S = Card->AddChildToVerticalBox(Stripe)) { S->SetPadding(FMargin(0.f, 1.f)); }
		}
		ResultsCountdown = MakeText(Tree, TEXT("ResultsCountdown"), FText::GetEmpty(), TEXT("Regular"), 17, TextDim);
		if (UVerticalBoxSlot* S = Card->AddChildToVerticalBox(ResultsCountdown)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f)); }

		UBorder* CardPanel = MakePanel(Tree, Card, FLinearColor(0.012f, 0.045f, 0.08f, 0.9f), 26.f, FMargin(36.f, 26.f), Sand * FLinearColor(1.f, 1.f, 1.f, 0.5f), 2.f);
		UBorder* Dim = MakePanel(Tree, CardPanel, FLinearColor(0.f, 0.f, 0.f, 0.45f), 0.f, FMargin(0.f), FLinearColor::Transparent, 0.f);
		Dim->SetHorizontalAlignment(HAlign_Center);
		Dim->SetVerticalAlignment(VAlign_Center);
		ResultsOverlay = Dim;
		ResultsOverlay->SetVisibility(ESlateVisibility::Collapsed);
		UCanvasPanelSlot* DimSlot = Canvas->AddChildToCanvas(Dim);
		DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DimSlot->SetOffsets(FMargin(0.f));
	}
}
