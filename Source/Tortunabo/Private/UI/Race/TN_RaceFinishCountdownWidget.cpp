#include "UI/Race/TN_RaceFinishCountdownWidget.h"
#include "TN_RaceArt.h"
#include "TN_RaceUIKit.h"
#include "UI/Race/TN_RaceCueSynthComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"

// Con nombre (no anónimo): en la compilación por bloques (unity) los nombres de un espacio anónimo se ven en el resto
// del bloque.
namespace TNRaceCountdownDetail
{
	/** Medallón del número (px a 1080 p) y alto desde arriba de la pantalla. */
	constexpr float MedalSize = 176.f;
	constexpr float MedalY = 262.f;

	/** Rejilla de los «¡toc!»: cada segundo; cada medio desde los 5 s; cada cuarto desde los 2,5 s. */
	float TickGrid(float Left)
	{
		return Left <= 2.5f ? 0.25f : (Left <= 5.f ? 0.5f : 1.f);
	}
}

void UTN_RaceFinishCountdownWidget::NativeOnInitialized()
{
	BuildTree();
	Super::NativeOnInitialized();
}

void UTN_RaceFinishCountdownWidget::BuildTree()
{
	using namespace TNRaceCountdownDetail;
	if (!WidgetTree || Canvas) { return; }
	UWidgetTree* Tree = WidgetTree;
	Canvas = TNRaceUI::Make<UCanvasPanel>(Tree, TEXT("RaceCountdownCanvas"));
	Tree->RootWidget = Canvas;
	// Por encima de la carrera, pero sin coger el ratón ni el teclado.
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// Cinta de arriba: la cara de la primera y «¡La primera ya está en el agua!»; debajo, quién se lleva qué.
	{
		UHorizontalBox* Row = TNRaceUI::Make<UHorizontalBox>(Tree);
		LeaderFace = TNRaceUI::MakeImage(Tree, TNHUDFaces::TurtleFace(ETNTurtleFace::Win), FVector2D(70.f, 70.f));
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(LeaderFace)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, -16.f, 10.f, -16.f)); }
		BannerText = TNRaceUI::MakeText(Tree, FText::GetEmpty(), TEXT("Bold"), 30, FLinearColor::White);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(BannerText)) { S->SetVerticalAlignment(VAlign_Center); }
		UBorder* Ribbon = TNRaceUI::MakeCard(Tree, TNHUDArt::RibbonTexture(), TNRaceUI::RibbonMargin, Row, FMargin(56.f, 16.f, 56.f, 20.f));
		Ribbon->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Banner = Ribbon;
		TNRaceUI::Place(Canvas, Ribbon, FVector2D(0.5f, 0.f), FVector2D(0.f, 92.f));

		SubText = TNRaceUI::MakeText(Tree, FText::GetEmpty(), TEXT("Bold"), 20, TNHUDArt::Ink, false);
		UBorder* Tag = TNRaceUI::MakeCard(Tree, TNHUDArt::SandTagTexture(), TNRaceUI::TagMargin, SubText, FMargin(30.f, 11.f, 30.f, 13.f));
		SubTag = Tag;
		TNRaceUI::Place(Canvas, Tag, FVector2D(0.5f, 0.f), FVector2D(0.f, 168.f));
	}

	// El número, en un medallón azul marino con aro de arena y un brillo detrás; «¡TIEMPO!» ocupa su sitio al final.
	{
		UOverlay* MedalBox = TNRaceUI::Make<UOverlay>(Tree);
		MedalGlow = TNRaceUI::MakeImage(Tree, TNRaceArt::SoftGlow(), FVector2D(MedalSize * 1.9f, MedalSize * 1.9f));
		MedalGlow->SetColorAndOpacity(TNHUDArt::Gold);
		TNRaceUI::AddAt(MedalBox, MedalGlow, HAlign_Center, VAlign_Center);
		UImage* Disc = TNRaceUI::Make<UImage>(Tree);
		Disc->SetBrush(TNHUDStyle::Rounded(TNHUDArt::Navy, MedalSize * 0.5f, TNHUDArt::SandC, 7.f));
		TNRaceUI::AddAt(MedalBox, TNRaceUI::MakeSize(Tree, Disc, MedalSize, MedalSize), HAlign_Center, VAlign_Center);
		NumberText = TNRaceUI::MakeText(Tree, FText::GetEmpty(), TEXT("Black"), 96, TNHUDArt::Cream);
		NumberText->SetJustification(ETextJustify::Center);
		TNRaceUI::AddAt(MedalBox, NumberText, HAlign_Center, VAlign_Center, FMargin(0.f, 0.f, 0.f, 6.f));
		MedalBox->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Medal = MedalBox;
		TNRaceUI::Place(Canvas, TNRaceUI::MakeSize(Tree, MedalBox, MedalSize * 1.9f, MedalSize * 1.9f), FVector2D(0.5f, 0.f), FVector2D(0.f, MedalY - MedalSize * 0.45f));

		TimeUpText = TNRaceUI::MakeText(Tree, FText::GetEmpty(), TEXT("Black"), 84, TNHUDArt::CoralC);
		TimeUpText->SetJustification(ETextJustify::Center);
		TimeUpText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		TimeUpText->SetVisibility(ESlateVisibility::Collapsed);
		TNRaceUI::Place(Canvas, TimeUpText, FVector2D(0.5f, 0.f), FVector2D(0.f, MedalY + 10.f));
	}

	// Lo que te toca, en una etiqueta de arena debajo del número.
	StatusText = TNRaceUI::MakeText(Tree, FText::GetEmpty(), TEXT("Bold"), 24, TNHUDArt::Ink, false);
	UBorder* StatusCard = TNRaceUI::MakeCard(Tree, TNHUDArt::SandTagTexture(), TNRaceUI::TagMargin, StatusText, FMargin(34.f, 14.f, 34.f, 16.f));
	StatusCard->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	StatusTag = StatusCard;
	TNRaceUI::Place(Canvas, StatusCard, FVector2D(0.5f, 0.f), FVector2D(0.f, MedalY + MedalSize * 0.62f + 64.f));

	UBorder* Preview = TNRaceUI::MakeCard(Tree, TNHUDArt::RibbonTexture(), TNRaceUI::RibbonMargin,
		TNRaceUI::MakeText(Tree, NSLOCTEXT("TNRace", "Preview", "VISTA PREVIA"), TEXT("Bold"), 15, FLinearColor::White), FMargin(34.f, 14.f, 34.f, 16.f));
	Preview->SetVisibility(ESlateVisibility::Collapsed);
	PreviewTag = Preview;
	TNRaceUI::Place(Canvas, Preview, FVector2D(0.f, 0.f), FVector2D(24.f, 24.f));
	SetRenderOpacity(0.f);
}

void UTN_RaceFinishCountdownWidget::SetView(const FTNRaceCountdownView& InView)
{
	BuildTree();
	View = InView;
	View.TotalSeconds = FMath::Max(1.f, View.TotalSeconds);
	View.SecondsLeft = FMath::Clamp(View.SecondsLeft, 0.f, View.TotalSeconds);
	if (!bHasView)
	{
		bHasView = true;
		LastLeft = View.SecondsLeft;
	}
	if (DismissAt >= 0.f && View.State != ETNBeachFinishCountdown::None)
	{
		// Vuelve a hacer falta (otra ronda con la cuenta en marcha): se queda.
		DismissAt = -1.f;
	}
	RefreshTexts();
}

void UTN_RaceFinishCountdownWidget::RefreshTexts()
{
	if (PreviewTag) { PreviewTag->SetVisibility(View.bPreview ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
	const bool bLeaderChanged = View.LeaderName != ShownLeader;
	if (bLeaderChanged || View.State != ShownState)
	{
		ShownLeader = View.LeaderName;
		const FText Leader = FText::FromString(View.LeaderName.IsEmpty() ? FString(TEXT("Tortuga")) : View.LeaderName);
		if (BannerText)
		{
			BannerText->SetText(View.State == ETNBeachFinishCountdown::AllIn
				? NSLOCTEXT("TNRace", "CountdownAllIn", "¡Todas en el agua!")
				: NSLOCTEXT("TNRace", "CountdownBanner", "¡La primera ya está en el agua!"));
		}
		if (SubText)
		{
			SubText->SetText(FText::Format(NSLOCTEXT("TNRace", "CountdownSub", "Concha para {0} · media concha para quien llegue antes del final"), Leader));
		}
		if (LeaderFace) { TNRaceUI::SetImageTexture(LeaderFace, TNRaceArt::TurtleFaceFor(this, View.LeaderLook, ETNTurtleFace::Win)); }
	}
	// Con «¡TIEMPO!», quien seguía corriendo se queda sin etiqueta: el centro, para el gusano de arena que se la come.
	const bool bOverNow = View.State == ETNBeachFinishCountdown::TimeUp || View.State == ETNBeachFinishCountdown::AllIn;
	const uint8 StatusKey = bOverNow && View.LocalStatus == 0 ? 3 : View.LocalStatus;
	if (StatusKey != ShownStatus && StatusText && StatusTag)
	{
		ShownStatus = StatusKey;
		switch (StatusKey)
		{
		case 0:
			StatusText->SetText(NSLOCTEXT("TNRace", "CountdownRun", "¡Corre! Media concha si llegas"));
			StatusTag->SetVisibility(ESlateVisibility::HitTestInvisible);
			break;
		case 1:
			StatusText->SetText(NSLOCTEXT("TNRace", "CountdownFirst", "¡Concha entera para ti!"));
			StatusTag->SetVisibility(ESlateVisibility::HitTestInvisible);
			break;
		case 2:
			StatusText->SetText(NSLOCTEXT("TNRace", "CountdownHalf", "¡Media concha para ti!"));
			StatusTag->SetVisibility(ESlateVisibility::HitTestInvisible);
			break;
		default:
			StatusTag->SetVisibility(ESlateVisibility::Collapsed);
			break;
		}
	}
	if (View.State != ShownState)
	{
		ShownState = View.State;
		const bool bOver = View.State == ETNBeachFinishCountdown::TimeUp || View.State == ETNBeachFinishCountdown::AllIn;
		if (bOver && TimeUpAt < 0.f)
		{
			// «¡TIEMPO!» con el silbato del árbitro.
			TimeUpAt = Time;
			PlayCue(static_cast<uint8>(ETNRaceCue::TimeUp), 1.f, 0.9f);
		}
		if (TimeUpText)
		{
			TimeUpText->SetText(View.State == ETNBeachFinishCountdown::AllIn
				? NSLOCTEXT("TNRace", "CountdownAllInBig", "¡TODAS AL AGUA!")
				: NSLOCTEXT("TNRace", "CountdownTimeUp", "¡TIEMPO!"));
			TimeUpText->SetVisibility(bOver ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (Medal) { Medal->SetVisibility(bOver ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }
		if (!bOver) { TimeUpAt = -1.f; }
	}
}

void UTN_RaceFinishCountdownWidget::Dismiss()
{
	if (DismissAt < 0.f) { DismissAt = Time; }
}

void UTN_RaceFinishCountdownWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	using namespace TNRaceCountdownDetail;
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

	const float Left = View.SecondsLeft;
	const bool bCounting = View.State == ETNBeachFinishCountdown::Counting;
	if (bCounting)
	{
		// El número: el segundo que corre (10… 1), con un rebote al cambiar.
		const int32 Number = FMath::Clamp(FMath::CeilToInt(Left), 1, FMath::CeilToInt(View.TotalSeconds));
		if (Number != ShownNumber && NumberText)
		{
			ShownNumber = Number;
			NumberPopAt = Time;
			NumberText->SetText(FText::AsNumber(Number));
		}
		// «¡Toc!» en cada paso de la rejilla, que se estrecha al final; el del segundo entero, más fuerte y agudo.
		if (LastLeft >= 0.f && Left < LastLeft && Left > 0.f)
		{
			const float Grid = TickGrid(Left);
			if (FMath::FloorToInt(LastLeft / Grid) != FMath::FloorToInt(Left / Grid))
			{
				const bool bWhole = FMath::FloorToInt(LastLeft) != FMath::FloorToInt(Left);
				const float Pitch = (Left <= 3.f ? 1.25f : 1.f) * (bWhole ? 1.f : 0.9f);
				PlayCue(static_cast<uint8>(ETNRaceCue::Tick), Pitch, bWhole ? 0.95f : 0.6f);
			}
		}
		LastLeft = Left;

		// Medallón: late con cada número, se pone dorado a los 3 s y coral en el último, y tiembla al final.
		if (Medal)
		{
			const float Since = Time - NumberPopAt;
			const float Pop = 1.f + 0.22f * FMath::Exp(-Since * 7.f) * FMath::Cos(Since * 16.f);
			const float Shake = Left <= 3.f ? (3.2f - Left) * 1.6f : 0.f;
			Medal->SetRenderScale(FVector2D(Pop, Pop));
			Medal->SetRenderTranslation(FVector2D(Shake * FMath::Sin(Time * 47.f), 0.6f * Shake * FMath::Sin(Time * 39.f)));
			Medal->SetRenderTransformAngle(Left <= 3.f ? 2.f * FMath::Sin(Time * 23.f) : 0.f);
		}
		if (NumberText)
		{
			const FLinearColor Color = Left <= 1.f ? TNHUDArt::CoralLight : (Left <= 3.f ? TNHUDArt::Gold : TNHUDArt::Cream);
			NumberText->SetColorAndOpacity(FSlateColor(Color));
		}
		if (MedalGlow)
		{
			MedalGlow->SetColorAndOpacity(Left <= 3.f ? TNHUDArt::CoralC : TNHUDArt::Gold);
			MedalGlow->SetRenderOpacity(0.45f + 0.3f * FMath::Exp(-(Time - NumberPopAt) * 5.f));
		}
	}
	else if (TimeUpAt >= 0.f && TimeUpText)
	{
		// «¡TIEMPO!»: entra grande con un rebote y tiembla un poco; pasado el golpe, se encoge y sube un poco para dejar
		// ver la arena (los gusanos que se comen a las que no han llegado).
		const float Since = Time - TimeUpAt;
		const float Pop = TNRaceUI::PopIn(Since / 0.3f) * (1.f + 0.05f * FMath::Sin(Time * 6.f));
		const float Settle = FMath::SmoothStep(1.2f, 1.6f, Since);
		const float Scale = Pop * FMath::Lerp(1.f, 0.72f, Settle);
		TimeUpText->SetRenderScale(FVector2D(Scale, Scale));
		TimeUpText->SetRenderTranslation(FVector2D(0.f, -70.f * Settle));
		TimeUpText->SetRenderTransformAngle(-4.f + 3.f * FMath::Sin(Time * 9.f) * FMath::Exp(-Since * 2.f));
	}
	if (Banner)
	{
		Banner->SetRenderTransformAngle(1.2f * FMath::Sin(Time * 2.2f));
	}
	if (StatusTag)
	{
		StatusTag->SetRenderScale(FVector2D(1.f + 0.04f * FMath::Sin(Time * (View.LocalStatus == 0 ? 8.f : 3.f))));
	}
}

void UTN_RaceFinishCountdownWidget::PlayCue(uint8 Cue, float Pitch, float Volume)
{
	if (!Synth.IsValid()) { Synth = UTN_RaceCueSynthComponent::Attach2D(GetOwningPlayer()); }
	if (UTN_RaceCueSynthComponent* Comp = Synth.Get())
	{
		Comp->Play(static_cast<ETNRaceCue>(Cue), Pitch, Volume);
	}
}
