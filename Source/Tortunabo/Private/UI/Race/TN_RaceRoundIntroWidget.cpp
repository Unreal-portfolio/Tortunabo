#include "UI/Race/TN_RaceRoundIntroWidget.h"
#include "TN_RaceArt.h"
#include "TN_RaceUIKit.h"
#include "UI/Race/TN_RaceCueSynthComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"

// Con nombre (no anónimo): en la compilación por bloques (unity) los nombres de un espacio anónimo se ven en el resto
// del bloque.
namespace TNRaceRoundIntroDetail
{
	/** Colocación (px a 1080 p desde el centro de la pantalla: la unión de la cáscara pasa entre el título y el número). */
	constexpr float TitleY = -175.f;
	constexpr float QuipY = -64.f;
	constexpr float CountY = 168.f;
	constexpr float MedalSize = 176.f;
	/** Salida disparada del título al romperse la cáscara. */
	constexpr float DismissSeconds = 0.2f;
	constexpr int32 MaxBits = 80;
	/** Frases de ánimo de las rondas normales. */
	constexpr int32 QuipCount = 5;

	FText QuipFor(const FTNRaceRoundIntroSetup& Setup)
	{
		if (Setup.bSprint)
		{
			return NSLOCTEXT("TNRace", "RoundIntroSprint", "La primera en el agua se lleva la partida.");
		}
		if (Setup.bMatchPoint)
		{
			return NSLOCTEXT("TNRace", "RoundIntroMatchPoint", "¡Esta ronda puede coronar a una campeona!");
		}
		if (Setup.Round <= 1)
		{
			return NSLOCTEXT("TNRace", "RoundIntroNewMatch", "Partida nueva: todas las conchas a cero.");
		}
		switch (FMath::Abs(Setup.Quip) % QuipCount)
		{
		case 0: return NSLOCTEXT("TNRace", "RoundIntroQuip0", "La playa se ha vuelto a desordenar. ¡A por el agua!");
		case 1: return NSLOCTEXT("TNRace", "RoundIntroQuip1", "Las gaviotas han vuelto con hambre.");
		case 2: return NSLOCTEXT("TNRace", "RoundIntroQuip2", "Trampas nuevas, arena de siempre.");
		case 3: return NSLOCTEXT("TNRace", "RoundIntroQuip3", "La tormenta ya se está peinando.");
		default: return NSLOCTEXT("TNRace", "RoundIntroQuip4", "Nadie se acuerda de la ronda anterior. Bueno, casi nadie.");
		}
	}

	/** Color del número de la salida: 3 crema, 2 dorado, 1 coral. */
	FLinearColor CountColor(int32 Number)
	{
		return Number >= 3 ? TNHUDArt::Cream : (Number == 2 ? TNHUDArt::Gold : TNHUDArt::CoralC);
	}
}

int32 UTN_RaceRoundIntroWidget::NumQuips()
{
	return TNRaceRoundIntroDetail::QuipCount;
}

void UTN_RaceRoundIntroWidget::NativeOnInitialized()
{
	BuildTree();
	Super::NativeOnInitialized();
	// Encima de la cáscara, sin coger el ratón ni el teclado; no se ve hasta Play.
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(0.f);
}

void UTN_RaceRoundIntroWidget::BuildTree()
{
	using namespace TNRaceRoundIntroDetail;
	if (!WidgetTree || Canvas) { return; }
	UWidgetTree* Tree = WidgetTree;
	SolidBrush = FSlateColorBrush(FLinearColor::White);
	Canvas = TNRaceUI::Make<UCanvasPanel>(Tree, TEXT("RaceRoundIntroCanvas"));
	Tree->RootWidget = Canvas;

	// «RONDA N» enorme, por encima de la unión de la cáscara.
	TitleText = TNRaceUI::MakeText(Tree, FText::GetEmpty(), TEXT("Black"), 150, TNHUDArt::Gold);
	TitleText->SetJustification(ETextJustify::Center);
	TitleText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	TNRaceUI::Place(Canvas, TitleText, FVector2D(0.5f, 0.5f), FVector2D(0.f, TitleY));

	// La frase de ánimo en su etiqueta de arena.
	QuipText = TNRaceUI::MakeText(Tree, FText::GetEmpty(), TEXT("Bold"), 26, TNHUDArt::Ink, false);
	UBorder* Tag = TNRaceUI::MakeCard(Tree, TNHUDArt::SandTagTexture(), TNRaceUI::TagMargin, QuipText, FMargin(40.f, 12.f, 40.f, 15.f));
	Tag->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	QuipTag = Tag;
	TNRaceUI::Place(Canvas, Tag, FVector2D(0.5f, 0.5f), FVector2D(0.f, QuipY));

	// Por debajo de la unión: «Colocando la playa…» y, en su sitio, el medallón del 3, 2, 1.
	StatusText = TNRaceUI::MakeText(Tree, NSLOCTEXT("TNRace", "RoundIntroPreparing", "Colocando la playa…"), TEXT("Bold"), 30, TNHUDArt::Cream);
	StatusText->SetJustification(ETextJustify::Center);
	TNRaceUI::Place(Canvas, StatusText, FVector2D(0.5f, 0.5f), FVector2D(0.f, CountY));

	UOverlay* Medal = TNRaceUI::Make<UOverlay>(Tree);
	CountDisc = TNRaceUI::Make<UImage>(Tree);
	CountDisc->SetBrush(TNHUDStyle::Rounded(TNHUDArt::NavyDeep, MedalSize * 0.5f, TNHUDArt::Gold, 7.f));
	TNRaceUI::AddAt(Medal, TNRaceUI::MakeSize(Tree, CountDisc, MedalSize, MedalSize), HAlign_Center, VAlign_Center);
	CountText = TNRaceUI::MakeText(Tree, FText::GetEmpty(), TEXT("Black"), 110, TNHUDArt::Cream);
	CountText->SetJustification(ETextJustify::Center);
	TNRaceUI::AddAt(Medal, CountText, HAlign_Center, VAlign_Center, FMargin(0.f, 0.f, 0.f, 8.f));
	USizeBox* MedalBox = TNRaceUI::MakeSize(Tree, Medal, MedalSize, MedalSize);
	MedalBox->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	MedalBox->SetRenderOpacity(0.f);
	CountBox = MedalBox;
	TNRaceUI::Place(Canvas, MedalBox, FVector2D(0.5f, 0.5f), FVector2D(0.f, CountY));

	UBorder* Preview = TNRaceUI::MakeCard(Tree, TNHUDArt::RibbonTexture(), TNRaceUI::RibbonMargin,
		TNRaceUI::MakeText(Tree, NSLOCTEXT("TNRace", "Preview", "VISTA PREVIA"), TEXT("Bold"), 15, FLinearColor::White), FMargin(34.f, 14.f, 34.f, 16.f));
	Preview->SetVisibility(ESlateVisibility::Collapsed);
	PreviewTag = Preview;
	TNRaceUI::Place(Canvas, Preview, FVector2D(0.f, 0.f), FVector2D(24.f, 24.f));
}

void UTN_RaceRoundIntroWidget::Setup(const FTNRaceRoundIntroSetup& InSetup)
{
	BuildTree();
	if (bHasSetup && InSetup == IntroSetup)
	{
		return;
	}
	bHasSetup = true;
	IntroSetup = InSetup;
	ApplyTexts();
}

void UTN_RaceRoundIntroWidget::ApplyTexts()
{
	if (TitleText)
	{
		TitleText->SetText(IntroSetup.bSprint ? NSLOCTEXT("TNRace", "RoundIntroSprintTitle", "SPRINT FINAL")
			: FText::Format(NSLOCTEXT("TNRace", "RoundIntroTitle", "RONDA {0}"), FText::AsNumber(FMath::Max(1, IntroSetup.Round))));
		TitleText->SetColorAndOpacity(FSlateColor(IntroSetup.bSprint ? TNHUDArt::CoralLight : TNHUDArt::Gold));
	}
	if (QuipText)
	{
		QuipText->SetText(TNRaceRoundIntroDetail::QuipFor(IntroSetup));
	}
	if (PreviewTag)
	{
		PreviewTag->SetVisibility(IntroSetup.bPreview ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UTN_RaceRoundIntroWidget::Play()
{
	if (PlayAt >= 0.f)
	{
		return;
	}
	BuildTree();
	PlayAt = Time;
	bSlamPlayed = false;
}

void UTN_RaceRoundIntroWidget::ShowCount(int32 Number)
{
	if (Number <= 0 || Number == Count)
	{
		return;
	}
	Count = Number;
	CountAt = Time;
	if (CountText)
	{
		CountText->SetText(FText::AsNumber(Number));
		CountText->SetColorAndOpacity(FSlateColor(TNRaceRoundIntroDetail::CountColor(Number)));
	}
}

void UTN_RaceRoundIntroWidget::Dismiss()
{
	if (PlayAt < 0.f)
	{
		RemoveFromParent();
		return;
	}
	if (DismissAt < 0.f) { DismissAt = Time; }
}

void UTN_RaceRoundIntroWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	using namespace TNRaceRoundIntroDetail;
	Super::NativeTick(MyGeometry, InDeltaTime);
	const float Dt = FMath::Min(InDeltaTime, 0.1f);
	Time += Dt;
	const FVector2f Size = FVector2f(MyGeometry.GetLocalSize());
	if (Size.X > 1.f) { LocalSize = FVector2D(Size.X, Size.Y); }
	if (PlayAt < 0.f)
	{
		SetRenderOpacity(0.f);
		return;
	}
	const float T = Time - PlayAt;

	// Salida: el título sale disparado hacia delante y todo se funde.
	float Out = 0.f;
	if (DismissAt >= 0.f)
	{
		Out = FMath::Clamp((Time - DismissAt) / DismissSeconds, 0.f, 1.f);
		if (Out >= 1.f)
		{
			RemoveFromParent();
			return;
		}
	}
	SetRenderOpacity(FMath::Clamp(T / 0.08f, 0.f, 1.f) * (1.f - Out));
	if (Canvas)
	{
		const float Burst = 1.f + 0.3f * TNRaceUI::Smooth(Out);
		Canvas->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Canvas->SetRenderScale(FVector2D(Burst, Burst));
	}

	// El título entra de golpe desde muy grande («¡pum!» y trocitos de cáscara), se mece y da un respingo con cada número.
	if (!bSlamPlayed && T >= 0.1f)
	{
		bSlamPlayed = true;
		PlayCue(static_cast<uint8>(ETNRaceCue::Slam), IntroSetup.bSprint ? 0.9f : 1.f, 1.f);
		SpawnBits(FVector2D(0.5 * LocalSize.X, 0.5 * LocalSize.Y + TitleY), 26);
	}
	if (TitleText)
	{
		const float S = T - 0.02f;
		const float Land = TNRaceUI::Smooth(S / 0.16f);
		const float Bounce = S > 0.16f ? 0.12f * FMath::Exp(-(S - 0.16f) * 6.f) * FMath::Sin((S - 0.16f) * 22.f) : 0.f;
		const float Bump = CountAt >= 0.f ? 0.1f * FMath::Exp(-(Time - CountAt) * 9.f) : 0.f;
		const float Breath = 1.f + 0.03f * FMath::Sin(T * 3.f);
		const float Scale = (FMath::Lerp(2.4f, 1.f, Land) + Bounce + Bump) * Breath;
		TitleText->SetRenderOpacity(FMath::Clamp(S / 0.08f, 0.f, 1.f));
		TitleText->SetRenderScale(FVector2D(FMath::Max(0.01f, Scale), FMath::Max(0.01f, Scale)));
		TitleText->SetRenderTransformAngle((IntroSetup.bSprint ? -4.f : -2.f) + 2.f * FMath::Sin(T * 2.1f));
	}
	if (QuipTag)
	{
		const float In = TNRaceUI::PopIn((T - 0.32f) / 0.32f);
		QuipTag->SetRenderOpacity(FMath::Clamp((T - 0.32f) / 0.08f, 0.f, 1.f));
		QuipTag->SetRenderScale(FVector2D(FMath::Max(0.01f, In), FMath::Max(0.01f, In)));
	}
	// Mientras se prepara, «Colocando la playa…» latiendo; con la cuenta, el medallón con su número.
	if (StatusText)
	{
		const float In = FMath::Clamp((T - 0.55f) / 0.25f, 0.f, 1.f);
		const float Pulse = 0.7f + 0.3f * FMath::Sin(T * 4.f);
		StatusText->SetRenderOpacity(Count > 0 ? 0.f : In * Pulse);
	}
	if (CountBox)
	{
		if (Count <= 0 || CountAt < 0.f)
		{
			CountBox->SetRenderOpacity(0.f);
		}
		else
		{
			const float C = Time - CountAt;
			const float Land = TNRaceUI::Smooth(C / 0.14f);
			const float Bounce = C > 0.14f ? 0.15f * FMath::Exp(-(C - 0.14f) * 7.f) * FMath::Sin((C - 0.14f) * 24.f) : 0.f;
			const float Scale = FMath::Lerp(1.7f, 1.f, Land) + Bounce;
			CountBox->SetRenderOpacity(FMath::Clamp(C / 0.06f, 0.f, 1.f));
			CountBox->SetRenderScale(FVector2D(Scale, Scale));
			CountBox->SetRenderTransformAngle(Count == 1 ? 6.f * FMath::Sin(Time * 30.f) * FMath::Exp(-C * 3.f) : 0.f);
		}
	}

	// Trocitos de cáscara: saltan, giran, caen y se apagan.
	for (int32 i = Bits.Num() - 1; i >= 0; --i)
	{
		FTNRoundIntroBit& Bit = Bits[i];
		Bit.Age += Dt;
		Bit.Vel.Y += 1100.f * Dt;
		Bit.Pos += Bit.Vel * Dt;
		Bit.Angle += Bit.Spin * Dt;
		if (Bit.Age >= Bit.Life || Bit.Pos.Y > LocalSize.Y + 40.f)
		{
			Bits.RemoveAtSwap(i);
		}
	}
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UTN_RaceRoundIntroWidget::SpawnBits(const FVector2D& Where, int32 Num)
{
	using namespace TNRaceRoundIntroDetail;
	static const FLinearColor Tints[] = { TNHUDArt::Hex(0x3A3F60), TNHUDArt::Hex(0x4B5585), TNHUDArt::Hex(0xFFC766), TNHUDArt::Cream };
	for (int32 k = 0; k < Num && Bits.Num() < MaxBits; ++k)
	{
		FTNRoundIntroBit Bit;
		Bit.Pos = Where + FVector2D(FMath::FRandRange(-300.f, 300.f), FMath::FRandRange(-40.f, 40.f));
		const float A = FMath::FRandRange(0.f, 2.f * PI);
		const float V = FMath::FRandRange(180.f, 560.f);
		Bit.Vel = FVector2D(FMath::Cos(A) * V, FMath::Sin(A) * V - 320.f);
		Bit.Size = FMath::FRandRange(6.f, 15.f);
		Bit.Angle = FMath::FRandRange(0.f, 360.f);
		Bit.Spin = FMath::FRandRange(-500.f, 500.f);
		Bit.Life = FMath::FRandRange(0.8f, 1.4f);
		Bit.Tint = Tints[FMath::RandRange(0, 3)];
		Bits.Add(Bit);
	}
}

void UTN_RaceRoundIntroWidget::PlayCue(uint8 Cue, float Pitch, float Volume)
{
	if (!Synth.IsValid()) { Synth = UTN_RaceCueSynthComponent::Attach2D(GetOwningPlayer()); }
	if (UTN_RaceCueSynthComponent* Comp = Synth.Get())
	{
		Comp->Play(static_cast<ETNRaceCue>(Cue), Pitch, Volume);
	}
}

int32 UTN_RaceRoundIntroWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
	for (const FTNRoundIntroBit& Bit : Bits)
	{
		const FVector2f BitSize(Bit.Size, Bit.Size * 0.8f);
		const FVector2f TopLeft(static_cast<float>(Bit.Pos.X) - 0.5f * BitSize.X, static_cast<float>(Bit.Pos.Y) - 0.5f * BitSize.Y);
		const float Fade = FMath::Clamp((Bit.Life - Bit.Age) / 0.3f, 0.f, 1.f);
		FSlateDrawElement::MakeRotatedBox(OutDrawElements, Layer + 1, AllottedGeometry.ToPaintGeometry(BitSize, FSlateLayoutTransform(TopLeft)), &SolidBrush,
			ESlateDrawEffect::None, FMath::DegreesToRadians(Bit.Angle), TOptional<FVector2f>(), FSlateDrawElement::RelativeToElement,
			FLinearColor(Bit.Tint.R, Bit.Tint.G, Bit.Tint.B, Bit.Tint.A * Fade) * Tint);
	}
	return Layer + 1;
}
