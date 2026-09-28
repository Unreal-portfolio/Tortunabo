#include "UI/Race/TN_RaceSprintWidget.h"
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
namespace TNRaceSprintDetail
{
	/** Cara de cada finalista (hueco) y del «VS» entre ellas (px a 1080 p). */
	constexpr float FaceSlotW = 210.f;
	constexpr float VsSlotW = 110.f;
	constexpr float RowH = 300.f;
	constexpr float RingSize = 176.f;
	/** Entra la primera cara y, después, una cosa (cara o «VS») cada tanto. */
	constexpr float FirstSlamAt = 0.75f;
	constexpr float SlamStagger = 0.42f;
	/** Confeti y arena que caen (por segundo) y tope de papelitos a la vez. */
	constexpr float ConfettiRate = 26.f;
	constexpr int32 MaxConfetti = 220;

	/** Centro de un widget en el espacio de otro (false si aún no se ha colocado). */
	bool CenterIn(const UWidget* Widget, const FGeometry& MyGeometry, FVector2D& OutCenter)
	{
		if (!Widget) { return false; }
		const FGeometry& Geo = Widget->GetCachedGeometry();
		if (FVector2f(Geo.GetLocalSize()).X <= 1.f) { return false; }
		const FVector2f Abs = FVector2f(Geo.GetAbsolutePositionAtCoordinates(FVector2f(0.5f, 0.5f)));
		const FVector2f Local = FVector2f(MyGeometry.AbsoluteToLocal(Abs));
		OutCenter = FVector2D(Local.X, Local.Y);
		return true;
	}
}

void UTN_RaceSprintWidget::NativeOnInitialized()
{
	BuildTree();
	Super::NativeOnInitialized();
}

void UTN_RaceSprintWidget::BuildTree()
{
	if (!WidgetTree || Canvas) { return; }
	UWidgetTree* Tree = WidgetTree;
	SolidBrush = FSlateColorBrush(FLinearColor::White);
	Canvas = TNRaceUI::Make<UCanvasPanel>(Tree, TEXT("RaceSprintCanvas"));
	Tree->RootWidget = Canvas;

	// Arriba: «¡SPRINT FINAL!» enorme y, debajo, la cinta con el empate.
	TitleText = TNRaceUI::MakeText(Tree, NSLOCTEXT("TNRace", "SprintTitle", "¡SPRINT FINAL!"), TEXT("Black"), 104, TNHUDArt::Gold);
	TitleText->SetJustification(ETextJustify::Center);
	TitleText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	TitleBox = TitleText;
	TNRaceUI::Place(Canvas, TitleText, FVector2D(0.5f, 0.f), FVector2D(0.f, 70.f));

	TieText = TNRaceUI::MakeText(Tree, FText::GetEmpty(), TEXT("Bold"), 25, FLinearColor::White);
	TieText->SetJustification(ETextJustify::Center);
	TieText->SetAutoWrapText(true);
	UBorder* Ribbon = TNRaceUI::MakeCard(Tree, TNHUDArt::RibbonTexture(), TNRaceUI::RibbonMargin, TNRaceUI::MakeSize(Tree, TieText, 1100.f, 0.f),
		FMargin(70.f, 16.f, 70.f, 20.f));
	Ribbon->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	TieRibbon = Ribbon;
	TNRaceUI::Place(Canvas, Ribbon, FVector2D(0.5f, 0.f), FVector2D(0.f, 222.f));

	// En medio, las caras de las finalistas con «VS» entre ellas (se rellenan en Setup).
	FinalistRow = TNRaceUI::Make<UCanvasPanel>(Tree);

	// Abajo: lo que te toca y la cuenta de la fase.
	NoteText = TNRaceUI::MakeText(Tree, FText::GetEmpty(), TEXT("Bold"), 23, TNHUDArt::Ink, false);
	UBorder* Note = TNRaceUI::MakeCard(Tree, TNHUDArt::SandTagTexture(), TNRaceUI::TagMargin, NoteText, FMargin(36.f, 14.f, 36.f, 16.f));
	Note->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	NoteTag = Note;
	TNRaceUI::Place(Canvas, Note, FVector2D(0.5f, 1.f), FVector2D(0.f, -120.f));

	CountdownText = TNRaceUI::MakeText(Tree, FText::GetEmpty(), TEXT("Bold"), 22, TNHUDArt::Cream);
	UBorder* Tag = TNRaceUI::MakeCard(Tree, TNHUDArt::RibbonTexture(), TNRaceUI::RibbonMargin, CountdownText, FMargin(44.f, 12.f, 44.f, 14.f));
	Tag->SetVisibility(ESlateVisibility::Collapsed);
	CountdownTag = Tag;
	TNRaceUI::Place(Canvas, Tag, FVector2D(0.5f, 1.f), FVector2D(0.f, -36.f));

	UBorder* Preview = TNRaceUI::MakeCard(Tree, TNHUDArt::RibbonTexture(), TNRaceUI::RibbonMargin,
		TNRaceUI::MakeText(Tree, NSLOCTEXT("TNRace", "Preview", "VISTA PREVIA"), TEXT("Bold"), 15, FLinearColor::White), FMargin(34.f, 14.f, 34.f, 16.f));
	Preview->SetVisibility(ESlateVisibility::Collapsed);
	PreviewTag = Preview;
	TNRaceUI::Place(Canvas, Preview, FVector2D(0.f, 0.f), FVector2D(24.f, 24.f));
}

void UTN_RaceSprintWidget::Setup(const FTNRaceSprintSetup& InSetup)
{
	BuildTree();
	SprintSetup = InSetup;
	Time = 0.f;
	DismissAt = -1.f;
	bFanfarePlayed = false;
	ShownSeconds = -1;
	Confetti.Reset();
	ConfettiDebt = 0.f;
	SetRenderOpacity(0.f);

	const int32 Halves = FMath::Max(0, SprintSetup.TieHalves);
	const FText Shells = (Halves % 2)
		? FText::Format(NSLOCTEXT("TNRace", "SprintShellsHalf", "{0} conchas y media"), FText::AsNumber(Halves / 2))
		: FText::Format(NSLOCTEXT("TNRace", "SprintShells", "{0} conchas"), FText::AsNumber(Halves / 2));
	if (TieText)
	{
		TieText->SetText(FText::Format(NSLOCTEXT("TNRace", "SprintTie",
			"¡Empate a {0}! Solo corren las finalistas, desde la mitad de la playa: la primera en el agua se lleva la partida."), Shells));
	}
	if (NoteText)
	{
		NoteText->SetText(SprintSetup.bLocalFinalist
			? NSLOCTEXT("TNRace", "SprintYouRun", "¡Tú corres! Sal de tu huevo y a por el agua.")
			: NSLOCTEXT("TNRace", "SprintYouWatch", "Tú lo miras de fantasma: sigue a las finalistas y cambia de tortuga cuando quieras."));
	}
	if (PreviewTag) { PreviewTag->SetVisibility(SprintSetup.bPreview ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
	BuildFinalists();
}

void UTN_RaceSprintWidget::BuildFinalists()
{
	using namespace TNRaceSprintDetail;
	if (!FinalistRow || !Canvas) { return; }
	UWidgetTree* Tree = WidgetTree;
	FinalistRow->ClearChildren();
	SlamWidgets.Reset();
	SlamAt.Reset();
	SlamDone.Reset();
	const int32 Num = SprintSetup.Finalists.Num();
	const float RowW = Num * FaceSlotW + FMath::Max(0, Num - 1) * VsSlotW;
	for (int32 i = 0; i < Num; ++i)
	{
		const FTNRaceTallyRow& Row = SprintSetup.Finalists[i];
		const float X = i * (FaceSlotW + VsSlotW) + FaceSlotW * 0.5f;

		// La cara en su aro (del color de su caparazón o de su piel), con un brillo coral detrás y su nombre debajo.
		UOverlay* FaceBox = TNRaceUI::Make<UOverlay>(Tree);
		UImage* GlowImg = TNRaceUI::MakeImage(Tree, TNRaceArt::SoftGlow(), FVector2D(RingSize * 1.7f, RingSize * 1.7f));
		GlowImg->SetColorAndOpacity(FLinearColor(TNHUDArt::CoralC.R, TNHUDArt::CoralC.G, TNHUDArt::CoralC.B, 0.7f));
		TNRaceUI::AddAt(FaceBox, GlowImg, HAlign_Center, VAlign_Center);
		UImage* Ring = TNRaceUI::Make<UImage>(Tree);
		Ring->SetBrush(TNHUDStyle::Rounded(TNRaceArt::UIColorOf(this, Row.Look), RingSize * 0.5f, TNHUDArt::Navy, 6.f));
		TNRaceUI::AddAt(FaceBox, TNRaceUI::MakeSize(Tree, Ring, RingSize, RingSize), HAlign_Center, VAlign_Center);
		UImage* FaceImg = TNRaceUI::MakeImage(Tree, TNRaceArt::TurtleFaceFor(this, Row.Look, ETNTurtleFace::Win), FVector2D(RingSize - 8.f, RingSize - 8.f));
		TNRaceUI::AddAt(FaceBox, FaceImg, HAlign_Center, VAlign_Center, FMargin(0.f, 0.f, 0.f, 6.f));
		UTextBlock* NameText = TNRaceUI::MakeText(Tree, FText::FromString(Row.Name.IsEmpty() ? FString(TEXT("Tortuga")) : Row.Name), TEXT("Bold"), 22, TNHUDArt::Ink, false);
		UBorder* NameTag = TNRaceUI::MakeCard(Tree, TNHUDArt::SandTagTexture(), TNRaceUI::TagMargin, NameText, FMargin(28.f, 12.f, 28.f, 14.f));
		if (Row.bLocal) { NameTag->SetBrush(TNRaceUI::BoxBrush(TNHUDArt::SandTagTexture(), TNRaceUI::TagMargin, TNHUDArt::Hex(0xFFE08A))); }
		TNRaceUI::AddAt(FaceBox, NameTag, HAlign_Center, VAlign_Bottom, FMargin(0.f, 0.f, 0.f, -58.f));
		USizeBox* FaceSlot = TNRaceUI::MakeSize(Tree, FaceBox, FaceSlotW, FaceSlotW);
		FaceSlot->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		FaceSlot->SetRenderOpacity(0.f);
		TNRaceUI::PlaceAt(FinalistRow, FaceSlot, FVector2D(X, RowH * 0.42f), FVector2D(FaceSlotW, FaceSlotW), FVector2D(0.5f, 0.5f));
		SlamWidgets.Add(FaceSlot);

		// «VS» en un disco coral, torcido, entre esta cara y la siguiente.
		if (i + 1 < Num)
		{
			UOverlay* VsBox = TNRaceUI::Make<UOverlay>(Tree);
			UImage* Disc = TNRaceUI::Make<UImage>(Tree);
			Disc->SetBrush(TNHUDStyle::Rounded(TNHUDArt::CoralC, 48.f, TNHUDArt::Cream, 5.f));
			TNRaceUI::AddAt(VsBox, TNRaceUI::MakeSize(Tree, Disc, 96.f, 96.f), HAlign_Center, VAlign_Center);
			TNRaceUI::AddAt(VsBox, TNRaceUI::MakeText(Tree, NSLOCTEXT("TNRace", "SprintVs", "VS"), TEXT("Black"), 42, FLinearColor::White), HAlign_Center, VAlign_Center,
				FMargin(0.f, 0.f, 0.f, 4.f));
			USizeBox* VsSlot = TNRaceUI::MakeSize(Tree, VsBox, VsSlotW, VsSlotW);
			VsSlot->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			VsSlot->SetRenderOpacity(0.f);
			TNRaceUI::PlaceAt(FinalistRow, VsSlot, FVector2D(X + FaceSlotW * 0.5f + VsSlotW * 0.5f, RowH * 0.42f), FVector2D(VsSlotW, VsSlotW), FVector2D(0.5f, 0.5f));
			SlamWidgets.Add(VsSlot);
		}
	}
	for (int32 j = 0; j < SlamWidgets.Num(); ++j)
	{
		SlamAt.Add(FirstSlamAt + SlamStagger * j);
		SlamDone.Add(false);
	}
	// La fila, centrada algo por debajo de la mitad de la pantalla (una sola vez: el lienzo se reutiliza).
	if (!FinalistRow->GetParent())
	{
		TNRaceUI::Place(Canvas, TNRaceUI::MakeSize(Tree, FinalistRow, FMath::Max(RowW, FaceSlotW), RowH), FVector2D(0.5f, 0.5f), FVector2D(0.f, 70.f));
	}
	else if (USizeBox* RowBox = Cast<USizeBox>(FinalistRow->GetParent()))
	{
		RowBox->SetWidthOverride(FMath::Max(RowW, FaceSlotW));
	}
}

void UTN_RaceSprintWidget::SetSecondsLeft(float InSeconds)
{
	SecondsLeft = FMath::Max(0.f, InSeconds);
}

void UTN_RaceSprintWidget::Dismiss()
{
	if (DismissAt < 0.f) { DismissAt = Time; }
}

void UTN_RaceSprintWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	using namespace TNRaceSprintDetail;
	Super::NativeTick(MyGeometry, InDeltaTime);
	const float Dt = FMath::Min(InDeltaTime, 0.1f);
	Time += Dt;
	const FVector2f Size = FVector2f(MyGeometry.GetLocalSize());
	if (Size.X > 1.f) { LocalSize = FVector2D(Size.X, Size.Y); }

	float Opacity = FMath::Clamp(Time / 0.25f, 0.f, 1.f);
	if (DismissAt >= 0.f)
	{
		const float Out = FMath::Clamp((Time - DismissAt) / 0.35f, 0.f, 1.f);
		Opacity *= 1.f - Out;
		if (Out >= 1.f)
		{
			RemoveFromParent();
			return;
		}
	}
	SetRenderOpacity(Opacity);

	// La fanfarria, nada más salir, y una lluvia de confeti desde arriba.
	if (!bFanfarePlayed && Time >= 0.12f)
	{
		bFanfarePlayed = true;
		PlayCue(static_cast<uint8>(ETNRaceCue::Fanfare), 1.f, 1.f);
		SpawnConfetti(FVector2D(LocalSize.X * 0.5f, LocalSize.Y * 0.12f), 40, true);
	}

	// Título: entra de golpe (grande) y luego se mece.
	if (TitleBox)
	{
		const float In = TNRaceUI::PopIn((Time - 0.05f) / 0.45f);
		const float Breath = 1.f + 0.035f * FMath::Sin(Time * 3.f);
		TitleBox->SetRenderScale(FVector2D(In * Breath, In * Breath));
		TitleBox->SetRenderTransformAngle(-3.f + 2.f * FMath::Sin(Time * 2.1f));
		TitleBox->SetRenderOpacity(FMath::Clamp((Time - 0.05f) / 0.12f, 0.f, 1.f));
	}
	if (TieRibbon)
	{
		const float In = TNRaceUI::PopIn((Time - 0.4f) / 0.35f);
		TieRibbon->SetRenderScale(FVector2D(FMath::Max(0.01f, In), FMath::Max(0.01f, In)));
		TieRibbon->SetRenderOpacity(FMath::Clamp((Time - 0.4f) / 0.1f, 0.f, 1.f));
	}

	// Caras y «VS»: cada una entra de golpe desde muy grande, con un «¡pum!» y confeti, y después se balancea.
	for (int32 j = 0; j < SlamWidgets.Num(); ++j)
	{
		UWidget* Slam = SlamWidgets[j];
		if (!Slam || !SlamAt.IsValidIndex(j)) { continue; }
		const float T = Time - SlamAt[j];
		if (T < 0.f)
		{
			Slam->SetRenderOpacity(0.f);
			continue;
		}
		const bool bVs = (j % 2) == 1;
		if (!SlamDone[j])
		{
			SlamDone[j] = true;
			PlayCue(static_cast<uint8>(ETNRaceCue::Slam), bVs ? 1.35f : 1.f, bVs ? 0.8f : 1.f);
			FVector2D Where;
			if (CenterIn(Slam, GetCachedGeometry(), Where)) { SpawnConfetti(Where, bVs ? 10 : 18, true); }
		}
		const float Land = TNRaceUI::Smooth(T / 0.16f);
		const float Bounce = T > 0.16f ? 0.12f * FMath::Exp(-(T - 0.16f) * 6.f) * FMath::Sin((T - 0.16f) * 22.f) : 0.f;
		const float Scale = FMath::Lerp(2.4f, 1.f, Land) + Bounce;
		Slam->SetRenderOpacity(FMath::Clamp(T / 0.08f, 0.f, 1.f));
		Slam->SetRenderScale(FVector2D(Scale, Scale));
		Slam->SetRenderTransformAngle(bVs ? -8.f + 6.f * FMath::Sin(Time * 5.f + j) : 3.f * FMath::Sin(Time * 2.3f + j));
		Slam->SetRenderTranslation(FVector2D(0.f, bVs ? 0.f : -7.f * FMath::Abs(FMath::Sin(Time * 3.2f + j))));
	}

	if (NoteTag)
	{
		const float In = TNRaceUI::PopIn((Time - 1.2f) / 0.35f);
		NoteTag->SetRenderScale(FVector2D(FMath::Max(0.01f, In), FMath::Max(0.01f, In)));
		NoteTag->SetRenderOpacity(FMath::Clamp((Time - 1.2f) / 0.1f, 0.f, 1.f));
	}
	if (CountdownTag && CountdownText)
	{
		const int32 Seconds = FMath::CeilToInt(SecondsLeft);
		CountdownTag->SetVisibility(Seconds > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Seconds > 0 && Seconds != ShownSeconds)
		{
			ShownSeconds = Seconds;
			CountdownText->SetText(FText::Format(NSLOCTEXT("TNRace", "SprintStartsIn", "El sprint empieza en {0}"), FText::AsNumber(Seconds)));
		}
	}

	// Confeti y arena: nacen arriba, caen meciéndose y se van por abajo.
	ConfettiDebt += ConfettiRate * Dt;
	while (ConfettiDebt >= 1.f)
	{
		ConfettiDebt -= 1.f;
		SpawnConfetti(FVector2D(FMath::FRandRange(0.f, static_cast<float>(LocalSize.X)), -20.f), 1, false);
	}
	for (int32 i = Confetti.Num() - 1; i >= 0; --i)
	{
		FTNSprintConfetti& Piece = Confetti[i];
		Piece.Vel.Y = FMath::Min(Piece.Vel.Y + 420.f * Dt, 260.f + 60.f * Piece.Sway);
		Piece.Vel.X *= FMath::Max(0.f, 1.f - 2.f * Dt);
		Piece.Pos += Piece.Vel * Dt + FVector2D(28.f * FMath::Sin(Time * 2.2f + Piece.Sway * 6.f) * Dt, 0.f);
		Piece.Angle += Piece.Spin * Dt;
		if (Piece.Pos.Y > LocalSize.Y + 40.f)
		{
			Confetti.RemoveAtSwap(i);
		}
	}
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UTN_RaceSprintWidget::SpawnConfetti(const FVector2D& Where, int32 Count, bool bBurst)
{
	using namespace TNRaceSprintDetail;
	static const FLinearColor Tints[] = { TNHUDArt::Gold, TNHUDArt::CoralC, TNHUDArt::SeaLight, TNHUDArt::Hex(0xFF9AD8), TNHUDArt::Cream };
	for (int32 k = 0; k < Count && Confetti.Num() < MaxConfetti; ++k)
	{
		FTNSprintConfetti Piece;
		Piece.Pos = Where;
		// Un granito de arena de cada cuatro: pequeño y del color de la playa.
		const bool bSand = FMath::RandRange(0, 3) == 0;
		Piece.Size = bSand ? FVector2D(5.f, 5.f) : FVector2D(FMath::FRandRange(8.f, 13.f), FMath::FRandRange(14.f, 22.f));
		Piece.Tint = bSand ? TNHUDArt::SandC : Tints[FMath::RandRange(0, 4)];
		if (bBurst)
		{
			const float A = FMath::FRandRange(0.f, 2.f * PI);
			const float V = FMath::FRandRange(180.f, 520.f);
			Piece.Vel = FVector2D(FMath::Cos(A) * V, FMath::Sin(A) * V - 200.f);
		}
		else
		{
			Piece.Vel = FVector2D(FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(80.f, 180.f));
		}
		Piece.Angle = FMath::FRandRange(0.f, 360.f);
		Piece.Spin = FMath::FRandRange(-420.f, 420.f);
		Piece.Sway = FMath::FRandRange(0.f, 1.f);
		Confetti.Add(Piece);
	}
}

void UTN_RaceSprintWidget::PlayCue(uint8 Cue, float Pitch, float Volume)
{
	if (!Synth.IsValid()) { Synth = UTN_RaceCueSynthComponent::Attach2D(GetOwningPlayer()); }
	if (UTN_RaceCueSynthComponent* Comp = Synth.Get())
	{
		Comp->Play(static_cast<ETNRaceCue>(Cue), Pitch, Volume);
	}
}

int32 UTN_RaceSprintWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());

	// Fondo azul marino y rayos que giran despacio desde el centro, dorados y corales (por debajo de todo lo demás).
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), &SolidBrush, ESlateDrawEffect::None,
		TNHUDArt::NavyDeep * Tint);
	const FVector2f Center(Size.X * 0.5f, Size.Y * 0.52f);
	const float Length = Size.Size() * 1.25f;
	constexpr int32 NumRays = 12;
	for (int32 i = 0; i < NumRays; ++i)
	{
		const float Angle = Time * 0.12f + PI * static_cast<float>(i) / NumRays;
		const FLinearColor RayColor = (i % 2 == 0) ? FLinearColor(TNHUDArt::Gold.R, TNHUDArt::Gold.G, TNHUDArt::Gold.B, 0.10f)
			: FLinearColor(TNHUDArt::CoralC.R, TNHUDArt::CoralC.G, TNHUDArt::CoralC.B, 0.08f);
		const FVector2f RaySize(Length, 170.f);
		FSlateDrawElement::MakeRotatedBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(RaySize, FSlateLayoutTransform(Center - RaySize * 0.5f)),
			&SolidBrush, ESlateDrawEffect::None, Angle, TOptional<FVector2f>(), FSlateDrawElement::RelativeToElement, RayColor * Tint);
	}

	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 2, InWidgetStyle, bParentEnabled);

	// Confeti y arena por encima.
	for (const FTNSprintConfetti& Piece : Confetti)
	{
		const FVector2f PieceSize(static_cast<float>(Piece.Size.X), static_cast<float>(Piece.Size.Y));
		const FVector2f TopLeft(static_cast<float>(Piece.Pos.X) - 0.5f * PieceSize.X, static_cast<float>(Piece.Pos.Y) - 0.5f * PieceSize.Y);
		FSlateDrawElement::MakeRotatedBox(OutDrawElements, Layer + 1, AllottedGeometry.ToPaintGeometry(PieceSize, FSlateLayoutTransform(TopLeft)), &SolidBrush,
			ESlateDrawEffect::None, FMath::DegreesToRadians(Piece.Angle), TOptional<FVector2f>(), FSlateDrawElement::RelativeToElement, Piece.Tint * Tint);
	}
	return Layer + 1;
}
