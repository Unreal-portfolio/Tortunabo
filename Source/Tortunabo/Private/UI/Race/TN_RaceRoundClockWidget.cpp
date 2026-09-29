#include "UI/Race/TN_RaceRoundClockWidget.h"
#include "TN_RaceArt.h"
#include "TN_RaceUIKit.h"
#include "UI/Race/TN_RaceCueSynthComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"

// Con nombre (no anónimo): en la compilación por bloques (unity) los nombres de un espacio anónimo se ven en el resto
// del bloque.
namespace TNRaceRoundClockDetail
{
	/**
	 * La pastilla, arriba en el centro (px a 1080 p): donde el HUD pone la pista del mar en el mapa procedural, que en la
	 * playa no sale. Su centro, a tanto del centro de la pantalla, y su altura desde arriba.
	 */
	constexpr float PillCenterX = 0.f;
	constexpr float PillY = 30.f;
	/** La cinta del aviso: debajo de la pista y del cartel de la tormenta (que empieza a 118 px). */
	constexpr float WarningY = 250.f;
	/** Cuánto se ve la cinta del aviso (s). */
	constexpr float WarningSeconds = 4.f;
	/** Desde cuántos segundos el reloj se pone dorado y desde cuántos coral y latiendo, con un «¡toc!» por segundo. */
	constexpr float GoldFrom = 30.f;
	constexpr float CoralFrom = 10.f;

	/** «0:59», «1:00». */
	FText FormatClock(int32 Seconds)
	{
		const int32 Safe = FMath::Max(0, Seconds);
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), Safe / 60, Safe % 60));
	}
}

void UTN_RaceRoundClockWidget::NativeOnInitialized()
{
	BuildTree();
	Super::NativeOnInitialized();
}

void UTN_RaceRoundClockWidget::BuildTree()
{
	using namespace TNRaceRoundClockDetail;
	if (!WidgetTree || Canvas) { return; }
	UWidgetTree* Tree = WidgetTree;
	Canvas = TNRaceUI::Make<UCanvasPanel>(Tree, TEXT("RaceRoundClockCanvas"));
	Tree->RootWidget = Canvas;
	// Por encima de la carrera, pero sin coger el ratón ni el teclado.
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// Pastilla: el mar pequeño (a donde hay que llegar) y, al lado, «TIEMPO DE RONDA» con los segundos.
	{
		UHorizontalBox* Row = TNRaceUI::Make<UHorizontalBox>(Tree);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(TNRaceUI::MakeImage(Tree, TNHUDArt::SeaIcon(), FVector2D(52.f, 42.f))))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		}
		UVerticalBox* Col = TNRaceUI::Make<UVerticalBox>(Tree);
		UTextBlock* Label = TNRaceUI::MakeText(Tree, NSLOCTEXT("TNRace", "RoundClockLabel", "TIEMPO DE RONDA"), TEXT("Bold"), 13, TNHUDArt::SandC, false);
		if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Label)) { S->SetHorizontalAlignment(HAlign_Center); }
		ClockText = TNRaceUI::MakeText(Tree, FormatClock(60), TEXT("Black"), 38, TNHUDArt::Cream);
		ClockText->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(ClockText)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, -6.f, 0.f, 0.f)); }
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Col)) { S->SetVerticalAlignment(VAlign_Center); }

		UBorder* Card = TNRaceUI::Make<UBorder>(Tree);
		Card->SetBrush(TNHUDStyle::Rounded(TNHUDArt::Hex(0x12305A, 0.92f), 26.f, TNHUDArt::SandC, 3.f));
		Card->SetPadding(FMargin(16.f, 8.f, 22.f, 10.f));
		Card->SetContent(Row);
		Card->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Pill = Card;
		TNRaceUI::Place(Canvas, Card, FVector2D(0.5f, 0.f), FVector2D(PillCenterX, PillY));
	}

	// Cinta del aviso (a los 60 y a los 30 s) con lo que pasa si se acaba en una etiqueta de arena debajo.
	{
		UVerticalBox* Col = TNRaceUI::Make<UVerticalBox>(Tree);
		WarningText = TNRaceUI::MakeText(Tree, FText::GetEmpty(), TEXT("Bold"), 30, FLinearColor::White);
		UBorder* Ribbon = TNRaceUI::MakeCard(Tree, TNHUDArt::RibbonTexture(), TNRaceUI::RibbonMargin, WarningText, FMargin(56.f, 16.f, 56.f, 20.f));
		if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Ribbon)) { S->SetHorizontalAlignment(HAlign_Center); }
		WarningSubText = TNRaceUI::MakeText(Tree, FText::GetEmpty(), TEXT("Bold"), 19, TNHUDArt::Ink, false);
		UBorder* Tag = TNRaceUI::MakeCard(Tree, TNHUDArt::SandTagTexture(), TNRaceUI::TagMargin, WarningSubText, FMargin(30.f, 11.f, 30.f, 13.f));
		if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Tag)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f)); }
		Col->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Col->SetVisibility(ESlateVisibility::Collapsed);
		Warning = Col;
		TNRaceUI::Place(Canvas, Col, FVector2D(0.5f, 0.f), FVector2D(0.f, WarningY));
	}

	UBorder* Preview = TNRaceUI::MakeCard(Tree, TNHUDArt::RibbonTexture(), TNRaceUI::RibbonMargin,
		TNRaceUI::MakeText(Tree, NSLOCTEXT("TNRace", "ClockPreview", "VISTA PREVIA"), TEXT("Bold"), 15, FLinearColor::White), FMargin(34.f, 14.f, 34.f, 16.f));
	Preview->SetVisibility(ESlateVisibility::Collapsed);
	PreviewTag = Preview;
	TNRaceUI::Place(Canvas, Preview, FVector2D(0.f, 0.f), FVector2D(24.f, 24.f));
	SetRenderOpacity(0.f);
}

void UTN_RaceRoundClockWidget::SetView(const FTNRaceRoundClockView& InView)
{
	using namespace TNRaceRoundClockDetail;
	BuildTree();
	View = InView;
	View.SecondsLeft = FMath::Max(0.f, View.SecondsLeft);
	if (PreviewTag) { PreviewTag->SetVisibility(View.bPreview ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
	if (DismissAt >= 0.f)
	{
		// Vuelve a hacer falta (p. ej. otra vista previa): se queda.
		DismissAt = -1.f;
	}

	// Los segundos (redondeando hacia arriba: «0:01» hasta el final), con un rebote al cambiar y un «¡toc!» en los últimos.
	const int32 Seconds = FMath::CeilToInt(View.SecondsLeft);
	if (Seconds != ShownSeconds && ClockText)
	{
		const bool bFirst = ShownSeconds < 0;
		ShownSeconds = Seconds;
		SecondPopAt = Time;
		ClockText->SetText(FormatClock(Seconds));
		const FLinearColor Color = View.SecondsLeft <= CoralFrom ? TNHUDArt::CoralLight : (View.SecondsLeft <= GoldFrom ? TNHUDArt::Gold : TNHUDArt::Cream);
		ClockText->SetColorAndOpacity(FSlateColor(Color));
		if (!bFirst && Seconds > 0 && View.SecondsLeft <= CoralFrom)
		{
			PlayCue(static_cast<uint8>(ETNRaceCue::Tick), Seconds <= 3 ? 1.25f : 1.1f, 0.7f);
		}
	}

	// Avisos: al entrar (el último minuto) y a los 30 s. Si entra ya por debajo de 30 s (se ha unido tarde), el de los 30.
	const int32 Stage = View.SecondsLeft <= GoldFrom ? 2 : 1;
	if (Stage > WarnedStage)
	{
		WarnedStage = Stage;
		ShowWarning(Stage);
	}
}

void UTN_RaceRoundClockWidget::ShowWarning(int32 Stage)
{
	if (WarningText)
	{
		WarningText->SetText(Stage >= 2
			? NSLOCTEXT("TNRace", "RoundClock30", "¡Quedan 30 segundos!")
			: NSLOCTEXT("TNRace", "RoundClock60", "¡Queda 1 minuto!"));
	}
	if (WarningSubText)
	{
		WarningSubText->SetText(View.bSprint
			? NSLOCTEXT("TNRace", "RoundClockSprintRule", "Si nadie llega al agua, gana la más cerca del mar")
			: NSLOCTEXT("TNRace", "RoundClockRule", "Si nadie llega al agua, la concha es para la más cerca del mar"));
	}
	if (Warning) { Warning->SetVisibility(ESlateVisibility::HitTestInvisible); }
	WarningAt = Time;
	// Un golpe grave y un «¡toc!»: que se oiga aunque se esté mirando a otra parte.
	PlayCue(static_cast<uint8>(ETNRaceCue::Slam), 1.f, 0.7f);
	PlayCue(static_cast<uint8>(ETNRaceCue::Tick), Stage >= 2 ? 1.25f : 1.f, 0.9f);
}

void UTN_RaceRoundClockWidget::Dismiss()
{
	if (DismissAt < 0.f) { DismissAt = Time; }
}

void UTN_RaceRoundClockWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	using namespace TNRaceRoundClockDetail;
	Super::NativeTick(MyGeometry, InDeltaTime);
	const float Dt = FMath::Min(InDeltaTime, 0.1f);
	Time += Dt;

	// Entrada con un fundido rápido y salida con otro.
	float Opacity = FMath::Clamp(Time / 0.25f, 0.f, 1.f);
	if (DismissAt >= 0.f)
	{
		const float Out = FMath::Clamp((Time - DismissAt) / 0.3f, 0.f, 1.f);
		Opacity *= 1.f - Out;
		if (Out >= 1.f)
		{
			RemoveFromParent();
			return;
		}
	}
	SetRenderOpacity(Opacity);

	// Pastilla: rebote con cada segundo; en los últimos, late y tiembla un poco.
	if (Pill)
	{
		const float Since = Time - SecondPopAt;
		const bool bHurry = View.SecondsLeft <= CoralFrom;
		const float Pop = 1.f + (bHurry ? 0.16f : 0.06f) * FMath::Exp(-Since * 8.f) * FMath::Cos(Since * 16.f);
		Pill->SetRenderScale(FVector2D(Pop, Pop));
		Pill->SetRenderTransformAngle(bHurry ? 1.5f * FMath::Sin(Time * 21.f) : 0.f);
	}

	// Cinta del aviso: entra con un rebote, se balancea y se va con un fundido.
	if (Warning && WarningAt >= 0.f)
	{
		const float Since = Time - WarningAt;
		if (Since >= WarningSeconds)
		{
			Warning->SetVisibility(ESlateVisibility::Collapsed);
			WarningAt = -1.f;
		}
		else
		{
			const float Pop = TNRaceUI::PopIn(Since / 0.3f);
			const float Fade = 1.f - FMath::Clamp((Since - (WarningSeconds - 0.4f)) / 0.4f, 0.f, 1.f);
			Warning->SetRenderScale(FVector2D(Pop, Pop));
			Warning->SetRenderTransformAngle(1.2f * FMath::Sin(Time * 2.2f));
			Warning->SetRenderOpacity(Fade);
		}
	}
}

void UTN_RaceRoundClockWidget::PlayCue(uint8 Cue, float Pitch, float Volume)
{
	if (!Synth.IsValid()) { Synth = UTN_RaceCueSynthComponent::Attach2D(GetOwningPlayer()); }
	if (UTN_RaceCueSynthComponent* Comp = Synth.Get())
	{
		Comp->Play(static_cast<ETNRaceCue>(Cue), Pitch, Volume);
	}
}
