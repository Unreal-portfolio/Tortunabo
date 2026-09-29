#include "UI/Race/TN_RaceArrivalWidget.h"
#include "TN_RaceArrivalArt.h"
#include "TN_RaceArt.h"
#include "TN_RaceUIKit.h"
#include "Audio/TN_ScoreShellSynthComponent.h"
#include "UI/Race/TN_RaceCueSynthComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"

// Con nombre (no anónimo): en la compilación por bloques (unity) los nombres de un espacio anónimo se ven en el resto
// del bloque.
namespace TNRaceArrivalDetail
{
	/** Guion (s desde Play): el puesto entra de golpe, el premio cae y se posa, sale el mensaje y el nombre del premio. */
	constexpr float PlaceSlamAt = 0.16f;
	constexpr float PrizeDropAt = 0.42f;
	constexpr float PrizeFallSeconds = 0.33f;
	constexpr float MessageAt = 0.6f;
	constexpr float PrizeTagAt = 0.85f;
	/** La fanfarria del 1.º empieza antes para que su nota larga (0,465 s después) caiga con la corona. */
	constexpr float FanfareLead = 0.465f;
	/** Salida rápida al final (s antes de ContentSeconds) y la de Dismiss. */
	constexpr float ExitSeconds = 0.22f;
	constexpr float DismissSeconds = 0.18f;
	/** Tope de partículas a la vez. */
	constexpr int32 MaxParticles = 260;

	/** Clases de partícula (FTNArrivalParticle::Kind). */
	constexpr uint8 KindConfetti = 0;
	constexpr uint8 KindSparkle = 1;
	constexpr uint8 KindRain = 2;
	constexpr uint8 KindGrain = 3;

	/** Color del número del puesto: oro, plata y bronce; después, arena y grises cada vez más apagados. */
	FLinearColor PlaceColor(int32 Place)
	{
		switch (FMath::Clamp(Place, 1, 8))
		{
		case 1: return TNHUDArt::Gold;
		case 2: return TNHUDArt::Hex(0xE3ECF3);
		case 3: return TNHUDArt::Hex(0xE8A06A);
		case 4: return TNHUDArt::SandC;
		case 5: return TNHUDArt::Hex(0xCFC6B4);
		case 6: return TNHUDArt::Hex(0xB9BFC7);
		case 7: return TNHUDArt::Hex(0xA8B0BA);
		default: return TNHUDArt::Hex(0x9AA3AE);
		}
	}

	/** Rayos y brillo detrás de la corona, del color de su medalla. */
	FLinearColor MedalGlow(int32 Place)
	{
		return Place <= 1 ? TNHUDArt::Hex(0xFFD24A) : (Place == 2 ? TNHUDArt::Hex(0xDDE8F2) : TNHUDArt::Hex(0xE8A06A));
	}

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

// ─────────────────────────────────────────────────────────────────────────────
// Textos
// ─────────────────────────────────────────────────────────────────────────────

TArray<FText> UTN_RaceArrivalWidget::MessagesFor(int32 Place)
{
	// Alentadores en el podio y cada vez más burlones abajo. En femenino: la tortuga.
	switch (FMath::Clamp(Place, 1, 8))
	{
	case 1:
		return TArray<FText>{
			NSLOCTEXT("TNRace", "Arrival1a", "¡Reina de la playa! Hasta las gaviotas te hacen reverencias."),
			NSLOCTEXT("TNRace", "Arrival1b", "¡Primera! El mar te esperaba con la alfombra de espuma puesta."),
			NSLOCTEXT("TNRace", "Arrival1c", "¡Oro! Los cangrejos ya están tallando tu estatua de arena."),
			NSLOCTEXT("TNRace", "Arrival1d", "¡Nadie te ha visto ni la cola! Esa corona te queda de miedo.") };
	case 2:
		return TArray<FText>{
			NSLOCTEXT("TNRace", "Arrival2a", "¡Casi! La arena aún quema de tus pasos."),
			NSLOCTEXT("TNRace", "Arrival2b", "¡Plata! A un aletazo del oro: la próxima es tuya."),
			NSLOCTEXT("TNRace", "Arrival2c", "¡Segunda y brillando! Hasta el sol se ha puesto celoso.") };
	case 3:
		return TArray<FText>{
			NSLOCTEXT("TNRace", "Arrival3a", "Podio. La corona es pequeña; el orgullo, no."),
			NSLOCTEXT("TNRace", "Arrival3b", "¡Bronce! Te cabe en una aleta, pero es toda tuya."),
			NSLOCTEXT("TNRace", "Arrival3c", "Tercera: corona de bolsillo, sonrisa de campeona.") };
	case 4:
		return TArray<FText>{
			NSLOCTEXT("TNRace", "Arrival4a", "Cuarta. El cubo también es un trono… de arena."),
			NSLOCTEXT("TNRace", "Arrival4b", "Cuarta. A un paso del podio, con un cubo de sombrero."),
			NSLOCTEXT("TNRace", "Arrival4c", "Cuarta. Si entornas los ojos, el cubo casi parece una corona.") };
	case 5:
		return TArray<FText>{
			NSLOCTEXT("TNRace", "Arrival5a", "Quinta. Ni frío ni calor: templadita."),
			NSLOCTEXT("TNRace", "Arrival5b", "Quinta. Media concha rota, medio aplauso."),
			NSLOCTEXT("TNRace", "Arrival5c", "Quinta. Justo en el medio, donde nadie mira.") };
	case 6:
		return TArray<FText>{
			NSLOCTEXT("TNRace", "Arrival6a", "Sexta. Hasta el flotador se rindió antes que tú."),
			NSLOCTEXT("TNRace", "Arrival6b", "Sexta. Llegas desinflada, como tu premio."),
			NSLOCTEXT("TNRace", "Arrival6c", "Sexta. Psssss… eso que se escapa es tu orgullo.") };
	case 7:
		return TArray<FText>{
			NSLOCTEXT("TNRace", "Arrival7a", "Séptima. El gusano ya había reservado mesa."),
			NSLOCTEXT("TNRace", "Arrival7b", "Séptima. Tu premio huele igual que tu carrera."),
			NSLOCTEXT("TNRace", "Arrival7c", "Séptima. Un calcetín con arena: útil para… nada.") };
	default:
		return TArray<FText>{
			NSLOCTEXT("TNRace", "Arrival8a", "Octava. Las gaviotas ya te llaman por tu nombre."),
			NSLOCTEXT("TNRace", "Arrival8b", "Octava. Te has traído medio mar enganchado a la cabeza."),
			NSLOCTEXT("TNRace", "Arrival8c", "Octava. Última, pero has llegado. Poca cosa, pero algo.") };
	}
}

FText UTN_RaceArrivalWidget::PrizeNameFor(int32 Place)
{
	switch (FMath::Clamp(Place, 1, 8))
	{
	case 1: return NSLOCTEXT("TNRace", "Prize1", "Corona de oro");
	case 2: return NSLOCTEXT("TNRace", "Prize2", "Corona de plata");
	case 3: return NSLOCTEXT("TNRace", "Prize3", "Corona de bronce (talla mini)");
	case 4: return NSLOCTEXT("TNRace", "Prize4", "Cubo de playa (del revés)");
	case 5: return NSLOCTEXT("TNRace", "Prize5", "Media concha rota");
	case 6: return NSLOCTEXT("TNRace", "Prize6", "Flotador pinchado");
	case 7: return NSLOCTEXT("TNRace", "Prize7", "Calcetín mojado con arena");
	default: return NSLOCTEXT("TNRace", "Prize8", "Alga de peluca");
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Montaje
// ─────────────────────────────────────────────────────────────────────────────

void UTN_RaceArrivalWidget::NativeOnInitialized()
{
	BuildTree();
	Super::NativeOnInitialized();
	// Encima de la cáscara, sin coger el ratón ni el teclado; no se ve hasta Play.
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(0.f);
}

void UTN_RaceArrivalWidget::BuildTree()
{
	if (!WidgetTree || Canvas) { return; }
	UWidgetTree* Tree = WidgetTree;
	SolidBrush = FSlateColorBrush(FLinearColor::White);
	GlowBrush = TNRaceUI::TextureBrush(TNRaceArt::SoftGlow(), FVector2D(128.0, 128.0));
	SparkBrush = TNRaceUI::TextureBrush(TNRaceArt::Sparkle(), FVector2D(64.0, 64.0));
	CloudBrush = TNRaceUI::TextureBrush(TNRaceArt::Cloud(), FVector2D(256.0, 128.0));
	Canvas = TNRaceUI::Make<UCanvasPanel>(Tree, TEXT("RaceArrivalCanvas"));
	Tree->RootWidget = Canvas;

	// Arriba: la cinta de la ronda, «HAS QUEDADO» y el puesto enorme.
	RoundText = TNRaceUI::MakeText(Tree, FText::GetEmpty(), TEXT("Bold"), 22, FLinearColor::White);
	UBorder* Ribbon = TNRaceUI::MakeCard(Tree, TNHUDArt::RibbonTexture(), TNRaceUI::RibbonMargin, RoundText, FMargin(48.f, 12.f, 48.f, 16.f));
	Ribbon->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	RoundRibbon = Ribbon;
	TNRaceUI::Place(Canvas, Ribbon, FVector2D(0.5f, 0.f), FVector2D(0.f, 40.f));

	HeadText = TNRaceUI::MakeText(Tree, NSLOCTEXT("TNRace", "ArrivalHead", "HAS QUEDADO"), TEXT("Black"), 44, TNHUDArt::Cream);
	HeadText->SetJustification(ETextJustify::Center);
	HeadText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	TNRaceUI::Place(Canvas, HeadText, FVector2D(0.5f, 0.f), FVector2D(0.f, 116.f));

	PlaceText = TNRaceUI::MakeText(Tree, FText::GetEmpty(), TEXT("Black"), 170, TNHUDArt::Gold);
	PlaceText->SetJustification(ETextJustify::Center);
	PlaceText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	TNRaceUI::Place(Canvas, PlaceText, FVector2D(0.5f, 0.f), FVector2D(0.f, 164.f));

	// En medio, el premio (su tamaño y su dibujo se ponen en Play; el giro y el aplastado, desde abajo).
	PrizeImage = TNRaceUI::Make<UImage>(Tree);
	USizeBox* PrizeSizeBox = TNRaceUI::MakeSize(Tree, PrizeImage, 320.f, 320.f);
	PrizeSizeBox->SetRenderTransformPivot(FVector2D(0.5f, 0.9f));
	PrizeBox = PrizeSizeBox;
	TNRaceUI::Place(Canvas, PrizeSizeBox, FVector2D(0.5f, 0.5f), FVector2D(0.f, 88.f));

	// Abajo: el nombre del premio en su etiqueta de arena y el mensaje.
	PrizeText = TNRaceUI::MakeText(Tree, FText::GetEmpty(), TEXT("Bold"), 24, TNHUDArt::Ink, false);
	UBorder* Tag = TNRaceUI::MakeCard(Tree, TNHUDArt::SandTagTexture(), TNRaceUI::TagMargin, PrizeText, FMargin(34.f, 11.f, 34.f, 14.f));
	Tag->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	PrizeTag = Tag;
	TNRaceUI::Place(Canvas, Tag, FVector2D(0.5f, 1.f), FVector2D(0.f, -168.f));

	MessageText = TNRaceUI::MakeText(Tree, FText::GetEmpty(), TEXT("Bold"), 34, TNHUDArt::Cream);
	MessageText->SetJustification(ETextJustify::Center);
	MessageText->SetAutoWrapText(true);
	MessageText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	TNRaceUI::Place(Canvas, TNRaceUI::MakeSize(Tree, MessageText, 1250.f, 0.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -56.f));

	UBorder* Preview = TNRaceUI::MakeCard(Tree, TNHUDArt::RibbonTexture(), TNRaceUI::RibbonMargin,
		TNRaceUI::MakeText(Tree, NSLOCTEXT("TNRace", "Preview", "VISTA PREVIA"), TEXT("Bold"), 15, FLinearColor::White), FMargin(34.f, 14.f, 34.f, 16.f));
	Preview->SetVisibility(ESlateVisibility::Collapsed);
	PreviewTag = Preview;
	TNRaceUI::Place(Canvas, Preview, FVector2D(0.f, 0.f), FVector2D(24.f, 24.f));
}

void UTN_RaceArrivalWidget::Play(const FTNRaceArrivalSetup& InSetup)
{
	BuildTree();
	ArrivalSetup = InSetup;
	ArrivalSetup.Place = FMath::Clamp(InSetup.Place, 1, 8);
	Time = 0.f;
	DismissAt = -1.f;
	CuesFired = 0;
	RainDebt = 0.f;
	ConfettiDebt = 0.f;
	SparkDebt = 0.f;
	Particles.Reset();
	bPlaying = true;

	const int32 Place = ArrivalSetup.Place;
	const TNRaceArrivalArt::EPrize Prize = TNRaceArrivalArt::PrizeForPlace(Place);
	if (RoundText)
	{
		RoundText->SetText(ArrivalSetup.bSprint ? NSLOCTEXT("TNRace", "ArrivalSprint", "SPRINT FINAL")
			: FText::Format(NSLOCTEXT("TNRace", "ArrivalRound", "RONDA {0}"), FText::AsNumber(FMath::Max(1, ArrivalSetup.Round))));
	}
	if (PlaceText)
	{
		// El puesto con su ordinal (en otros idiomas, con su forma: p. ej. «{0}{0}|ordinal(one=st,two=nd,few=rd,other=th)»).
		PlaceText->SetText(FText::Format(NSLOCTEXT("TNRace", "ArrivalPlace", "{0}.º"), Place));
		PlaceText->SetColorAndOpacity(FSlateColor(TNRaceArrivalDetail::PlaceColor(Place)));
	}
	const FVector2D Size = TNRaceArrivalArt::PrizeSize(Prize);
	PrizeExtent = Size;
	if (PrizeImage)
	{
		PrizeImage->SetBrush(TNRaceUI::TextureBrush(TNRaceArrivalArt::PrizeTexture(Prize), Size));
	}
	if (USizeBox* SizeBox = Cast<USizeBox>(PrizeBox))
	{
		SizeBox->SetWidthOverride(static_cast<float>(Size.X));
		SizeBox->SetHeightOverride(static_cast<float>(Size.Y));
	}
	if (PrizeText)
	{
		PrizeText->SetText(PrizeNameFor(Place));
	}
	if (MessageText)
	{
		const TArray<FText> Messages = MessagesFor(Place);
		MessageText->SetText(Messages.Num() > 0 ? Messages[FMath::RandRange(0, Messages.Num() - 1)] : FText::GetEmpty());
	}
	if (PreviewTag)
	{
		PreviewTag->SetVisibility(ArrivalSetup.bPreview ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	// Todo entra por guion: nada a la vista en el primer fotograma.
	for (UWidget* Piece : { static_cast<UWidget*>(RoundRibbon.Get()), static_cast<UWidget*>(HeadText.Get()), static_cast<UWidget*>(PlaceText.Get()),
		PrizeBox.Get(), PrizeTag.Get(), static_cast<UWidget*>(MessageText.Get()) })
	{
		if (Piece) { Piece->SetRenderOpacity(0.f); }
	}
}

void UTN_RaceArrivalWidget::Dismiss()
{
	if (!bPlaying)
	{
		RemoveFromParent();
		return;
	}
	if (DismissAt < 0.f) { DismissAt = Time; }
}

// ─────────────────────────────────────────────────────────────────────────────
// Animación
// ─────────────────────────────────────────────────────────────────────────────

void UTN_RaceArrivalWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	using namespace TNRaceArrivalDetail;
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bPlaying)
	{
		SetRenderOpacity(0.f);
		return;
	}
	const float Dt = FMath::Min(InDeltaTime, 0.1f);
	Time += Dt;
	const FVector2f Size = FVector2f(MyGeometry.GetLocalSize());
	if (Size.X > 1.f) { LocalSize = FVector2D(Size.X, Size.Y); }

	// Salida: la rápida del final (encoge y se funde) o la de Dismiss.
	float Out = FMath::Clamp((Time - (ContentSeconds - ExitSeconds)) / ExitSeconds, 0.f, 1.f);
	if (DismissAt >= 0.f)
	{
		const float DismissOut = FMath::Clamp((Time - DismissAt) / DismissSeconds, 0.f, 1.f);
		Out = FMath::Max(Out, DismissOut);
		if (DismissOut >= 1.f)
		{
			RemoveFromParent();
			return;
		}
	}
	SetRenderOpacity(FMath::Clamp(Time / 0.1f, 0.f, 1.f) * (1.f - Out));
	if (Canvas)
	{
		const float Shrink = 1.f - 0.12f * TNRaceUI::Smooth(Out);
		Canvas->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Canvas->SetRenderScale(FVector2D(Shrink, Shrink));
	}
	if (PrizeBox)
	{
		FVector2D Center;
		if (CenterIn(PrizeBox, MyGeometry, Center)) { PrizeCenter = Center; }
	}
	FireCues();

	const int32 Place = ArrivalSetup.Place;
	const bool bPodium = Place <= 3;
	if (RoundRibbon)
	{
		const float In = TNRaceUI::PopIn(Time / 0.25f);
		RoundRibbon->SetRenderScale(FVector2D(FMath::Max(0.01f, In), FMath::Max(0.01f, In)));
		RoundRibbon->SetRenderOpacity(FMath::Clamp(Time / 0.08f, 0.f, 1.f));
	}
	if (HeadText)
	{
		const float In = FMath::Clamp((Time - 0.05f) / 0.15f, 0.f, 1.f);
		HeadText->SetRenderOpacity(In);
		HeadText->SetRenderTranslation(FVector2D(0.f, -24.f * (1.f - TNRaceUI::Smooth(In))));
	}
	// El puesto: entra de golpe desde muy grande y luego se mece; en el podio, contento; abajo, cada vez más torcido.
	if (PlaceText)
	{
		const float T = Time - PlaceSlamAt;
		const float Land = TNRaceUI::Smooth(T / 0.14f);
		const float Bounce = T > 0.14f ? 0.14f * FMath::Exp(-(T - 0.14f) * 6.f) * FMath::Sin((T - 0.14f) * 22.f) : 0.f;
		const float Scale = T < 0.f ? 0.01f : FMath::Lerp(2.6f, 1.f, Land) + Bounce;
		PlaceText->SetRenderOpacity(FMath::Clamp(T / 0.06f, 0.f, 1.f));
		PlaceText->SetRenderScale(FVector2D(FMath::Max(0.01f, Scale), FMath::Max(0.01f, Scale)));
		const float Sag = bPodium ? 0.f : 2.f * static_cast<float>(Place - 3);
		PlaceText->SetRenderTransformAngle(bPodium ? 3.f * FMath::Sin(Time * 3.f) : Sag + 2.f * FMath::Sin(Time * 1.6f));
	}
	AnimatePrize(Time - PrizeDropAt);
	if (MessageText)
	{
		const float T = Time - MessageAt;
		MessageText->SetRenderOpacity(FMath::Clamp(T / 0.2f, 0.f, 1.f));
		const float Pop = TNRaceUI::PopIn(T / 0.3f);
		MessageText->SetRenderScale(FVector2D(0.85f + 0.15f * Pop, 0.85f + 0.15f * Pop));
	}
	if (PrizeTag)
	{
		const float T = Time - PrizeTagAt;
		const float In = TNRaceUI::PopIn(T / 0.3f);
		PrizeTag->SetRenderOpacity(FMath::Clamp(T / 0.08f, 0.f, 1.f));
		PrizeTag->SetRenderScale(FVector2D(FMath::Max(0.01f, In), FMath::Max(0.01f, In)));
	}

	// Lo que no para: confeti del 1.º, destellos en las coronas y, del 6.º, la nubecita que llueve encima.
	const bool bLanded = Time >= PrizeDropAt + PrizeFallSeconds;
	if (bLanded && Place == 1)
	{
		ConfettiDebt += 24.f * Dt;
		while (ConfettiDebt >= 1.f)
		{
			ConfettiDebt -= 1.f;
			SpawnParticles(FVector2D(FMath::FRandRange(0.f, static_cast<float>(LocalSize.X)), -20.f), 1, KindConfetti, false);
		}
	}
	if (bLanded && bPodium)
	{
		SparkDebt += (Place == 1 ? 7.f : 4.f) * Dt;
		while (SparkDebt >= 1.f)
		{
			SparkDebt -= 1.f;
			const FVector2D Where = PrizeCenter + FVector2D(FMath::FRandRange(-0.45f, 0.45f) * PrizeExtent.X, FMath::FRandRange(-0.4f, 0.3f) * PrizeExtent.Y);
			SpawnParticles(Where, 1, KindSparkle, false);
		}
	}
	if (bLanded && Place >= 6)
	{
		RainDebt += (Place >= 8 ? 46.f : 30.f) * Dt;
		const FVector2D CloudBottom = PrizeCenter + FVector2D(0.f, -0.5f * PrizeExtent.Y - 40.f);
		while (RainDebt >= 1.f)
		{
			RainDebt -= 1.f;
			SpawnParticles(CloudBottom + FVector2D(FMath::FRandRange(-105.f, 105.f), 0.f), 1, KindRain, false);
		}
	}
	for (int32 i = Particles.Num() - 1; i >= 0; --i)
	{
		FTNArrivalParticle& Piece = Particles[i];
		Piece.Age += Dt;
		switch (Piece.Kind)
		{
		case KindConfetti:
			Piece.Vel.Y = FMath::Min(Piece.Vel.Y + 420.f * Dt, 260.f);
			Piece.Vel.X *= FMath::Max(0.f, 1.f - 2.f * Dt);
			Piece.Pos += Piece.Vel * Dt + FVector2D(26.f * FMath::Sin(Time * 2.2f + Piece.Spin * 0.01f) * Dt, 0.f);
			Piece.Angle += Piece.Spin * Dt;
			break;
		case KindSparkle:
		case KindRain:
			Piece.Pos += Piece.Vel * Dt;
			Piece.Angle += Piece.Spin * Dt;
			break;
		default:
			Piece.Vel.Y += 900.f * Dt;
			Piece.Pos += Piece.Vel * Dt;
			Piece.Angle += Piece.Spin * Dt;
			break;
		}
		if (Piece.Age >= Piece.Life || Piece.Pos.Y > LocalSize.Y + 60.f)
		{
			Particles.RemoveAtSwap(i);
		}
	}
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UTN_RaceArrivalWidget::AnimatePrize(float T)
{
	using namespace TNRaceArrivalDetail;
	if (!PrizeBox) { return; }
	if (T < 0.f)
	{
		PrizeBox->SetRenderOpacity(0.f);
		return;
	}
	PrizeBox->SetRenderOpacity(FMath::Clamp(T / 0.05f, 0.f, 1.f));
	const TNRaceArrivalArt::EPrize Prize = TNRaceArrivalArt::PrizeForPlace(ArrivalSetup.Place);
	const bool bCrown = ArrivalSetup.Place <= 3;
	// Cae de arriba cada vez más deprisa y, al posarse, se aplasta y rebota; luego, su reposo.
	const float Fall = FMath::Clamp(T / PrizeFallSeconds, 0.f, 1.f);
	float Y = -700.f * (1.f - Fall * Fall);
	float ScaleX = 1.f;
	float ScaleY = 1.f;
	float Angle = 0.f;
	const float Since = T - PrizeFallSeconds;
	if (Since < 0.f)
	{
		// En el aire: las coronas giran un poco; lo demás cae dando tumbos.
		Angle = (bCrown ? 12.f : -25.f) * (1.f - Fall);
	}
	else
	{
		const float Wobble = FMath::Exp(-Since * 7.f) * FMath::Cos(Since * 24.f);
		ScaleY = 1.f - 0.2f * Wobble;
		ScaleX = 1.f + 0.14f * Wobble;
		switch (Prize)
		{
		case TNRaceArrivalArt::EPrize::GoldCrown:
		case TNRaceArrivalArt::EPrize::SilverCrown:
		case TNRaceArrivalArt::EPrize::BronzeCrown:
			// Flota y se balancea, orgullosa.
			Y += -10.f * FMath::Sin(Since * 2.6f);
			Angle = 4.f * FMath::Sin(Since * 1.8f);
			break;
		case TNRaceArrivalArt::EPrize::Bucket:
			// Se tambalea y se queda un poco torcido.
			Angle = 3.f + 7.f * FMath::Sin(Since * 3.2f) * FMath::Exp(-Since * 0.8f);
			break;
		case TNRaceArrivalArt::EPrize::BrokenShell:
			Angle = -6.f + 3.f * FMath::Sin(Since * 1.4f);
			break;
		case TNRaceArrivalArt::EPrize::Floatie:
		{
			// Se va desinflando a golpecitos.
			const float Deflate = FMath::Min(1.f, Since / 1.5f);
			ScaleY *= 1.f - 0.06f * Deflate - 0.04f * (0.5f + 0.5f * FMath::Sin(Since * 6.f));
			ScaleX *= 1.f + 0.03f * Deflate;
			break;
		}
		case TNRaceArrivalArt::EPrize::Sock:
			Angle = 6.f * FMath::Sin(Since * 2.1f);
			break;
		default:
			// El alga, chorreando y mustia.
			Angle = 3.f * FMath::Sin(Since * 1.3f);
			Y += 4.f * FMath::Sin(Since * 2.f);
			break;
		}
	}
	PrizeBox->SetRenderTranslation(FVector2D(0.f, Y));
	PrizeBox->SetRenderScale(FVector2D(ScaleX, ScaleY));
	PrizeBox->SetRenderTransformAngle(Angle);
}

void UTN_RaceArrivalWidget::FireCues()
{
	using namespace TNRaceArrivalDetail;
	const int32 Place = ArrivalSetup.Place;
	const float LandAt = PrizeDropAt + PrizeFallSeconds;
	// 0: el golpe del puesto, más grave cuanto peor.
	if (CuesFired == 0 && Time >= PlaceSlamAt)
	{
		++CuesFired;
		PlayCue(static_cast<uint8>(ETNRaceCue::Slam), 1.08f - 0.05f * static_cast<float>(Place - 1), 1.f);
	}
	// 1: la fanfarria del 1.º, para que su nota larga caiga con la corona.
	if (CuesFired == 1 && Time >= LandAt - FanfareLead)
	{
		++CuesFired;
		if (Place == 1) { PlayCue(static_cast<uint8>(ETNRaceCue::Fanfare), 1.f, 0.95f); }
	}
	// 2: el premio se posa.
	if (CuesFired == 2 && Time >= LandAt)
	{
		++CuesFired;
		const FVector2D Landing = PrizeCenter + FVector2D(0.f, 0.35f * PrizeExtent.Y);
		switch (Place)
		{
		case 1:
			PlayShell(true, 3, 0.f, 1.f);
			SpawnParticles(PrizeCenter, 70, KindConfetti, true);
			SpawnParticles(PrizeCenter, 16, KindSparkle, true);
			break;
		case 2:
			PlayShell(true, 3, -2.f, 0.9f);
			SpawnParticles(PrizeCenter, 24, KindConfetti, true);
			SpawnParticles(PrizeCenter, 12, KindSparkle, true);
			break;
		case 3:
			PlayShell(true, 2, 0.f, 0.8f);
			SpawnParticles(PrizeCenter, 8, KindSparkle, true);
			break;
		case 4:
			PlayShell(false, 1, -3.f, 1.f);
			SpawnParticles(Landing, 18, KindGrain, true);
			break;
		default:
			PlayCue(static_cast<uint8>(ETNRaceCue::SadTrombone), 1.f - 0.045f * static_cast<float>(Place - 5), 1.f);
			SpawnParticles(Landing, 10, KindGrain, true);
			break;
		}
	}
	// 3: el segundo «pom» del 4.º, más grave (casi…).
	if (CuesFired == 3 && Time >= LandAt + 0.22f)
	{
		++CuesFired;
		if (Place == 4) { PlayShell(false, 1, -8.f, 0.9f); }
	}
}

void UTN_RaceArrivalWidget::SpawnParticles(const FVector2D& Where, int32 Count, uint8 Kind, bool bBurst)
{
	using namespace TNRaceArrivalDetail;
	static const FLinearColor ConfettiTints[] = { TNHUDArt::Gold, TNHUDArt::CoralC, TNHUDArt::SeaLight, TNHUDArt::Hex(0xFF9AD8), TNHUDArt::Cream };
	for (int32 k = 0; k < Count && Particles.Num() < MaxParticles; ++k)
	{
		FTNArrivalParticle Piece;
		Piece.Pos = Where;
		Piece.Kind = Kind;
		switch (Kind)
		{
		case KindConfetti:
		{
			Piece.Size = FVector2D(FMath::FRandRange(8.f, 13.f), FMath::FRandRange(14.f, 22.f));
			Piece.Tint = ConfettiTints[FMath::RandRange(0, 4)];
			Piece.Life = 4.f;
			if (bBurst)
			{
				const float A = FMath::FRandRange(0.f, 2.f * PI);
				const float V = FMath::FRandRange(200.f, 620.f);
				Piece.Vel = FVector2D(FMath::Cos(A) * V, FMath::Sin(A) * V - 260.f);
			}
			else
			{
				Piece.Vel = FVector2D(FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(80.f, 180.f));
			}
			Piece.Angle = FMath::FRandRange(0.f, 360.f);
			Piece.Spin = FMath::FRandRange(-420.f, 420.f);
			break;
		}
		case KindSparkle:
		{
			const float S = FMath::FRandRange(22.f, 46.f);
			Piece.Size = FVector2D(S, S);
			Piece.Tint = ArrivalSetup.Place == 1 ? TNHUDArt::Hex(0xFFF1B8) : FLinearColor::White;
			Piece.Life = FMath::FRandRange(0.45f, 0.8f);
			Piece.Vel = bBurst ? FVector2D(FMath::FRandRange(-260.f, 260.f), FMath::FRandRange(-260.f, 160.f)) : FVector2D(0.f, -12.f);
			Piece.Spin = FMath::FRandRange(-90.f, 90.f);
			break;
		}
		case KindRain:
			Piece.Size = FVector2D(3.f, FMath::FRandRange(14.f, 20.f));
			Piece.Tint = TNHUDArt::Hex(0xBFEFFA, 0.85f);
			Piece.Vel = FVector2D(-30.f, FMath::FRandRange(480.f, 560.f));
			Piece.Life = 0.6f;
			Piece.Angle = -3.5f;
			break;
		default:
		{
			// Granos de arena (4.º) o polvo gris (del 5.º en adelante) que saltan al posarse el premio.
			const float S = FMath::FRandRange(4.f, 8.f);
			Piece.Size = FVector2D(S, S);
			Piece.Tint = ArrivalSetup.Place <= 4 ? (FMath::RandRange(0, 2) == 0 ? TNHUDArt::WetSand : TNHUDArt::SandC) : TNHUDArt::Hex(0xC9C1B0, 0.9f);
			Piece.Vel = FVector2D(FMath::FRandRange(-220.f, 220.f), FMath::FRandRange(-360.f, -120.f));
			Piece.Life = 1.2f;
			Piece.Spin = FMath::FRandRange(-300.f, 300.f);
			break;
		}
		}
		Particles.Add(Piece);
	}
}

void UTN_RaceArrivalWidget::PlayCue(uint8 Cue, float Pitch, float Volume)
{
	if (!CueSynth.IsValid()) { CueSynth = UTN_RaceCueSynthComponent::Attach2D(GetOwningPlayer()); }
	if (UTN_RaceCueSynthComponent* Comp = CueSynth.Get())
	{
		Comp->Play(static_cast<ETNRaceCue>(Cue), Pitch, Volume);
	}
}

void UTN_RaceArrivalWidget::PlayShell(bool bPlin, uint8 Tier, float Semitones, float Volume)
{
	if (!ShellSynth.IsValid()) { ShellSynth = UTN_ScoreShellSynthComponent::Attach2D(GetOwningPlayer()); }
	if (UTN_ScoreShellSynthComponent* Comp = ShellSynth.Get())
	{
		Comp->TriggerSound(bPlin ? ETNScoreShellSound::Plin : ETNScoreShellSound::Pom, Tier, Semitones, Volume);
	}
}

int32 UTN_RaceArrivalWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	using namespace TNRaceArrivalDetail;
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
	const int32 Place = ArrivalSetup.Place;
	const float SinceLand = Time - (PrizeDropAt + PrizeFallSeconds);
	const FVector2f Center(static_cast<float>(PrizeCenter.X), static_cast<float>(PrizeCenter.Y));

	// Detrás del premio, desde que se posa: en el podio, rayos que giran y un brillo del color de su medalla; abajo, un
	// foco gris y mustio.
	if (bPlaying && SinceLand > -0.1f)
	{
		const float Grow = TNRaceUI::Smooth((SinceLand + 0.1f) / 0.35f);
		if (Place <= 3)
		{
			const FLinearColor Ray = MedalGlow(Place);
			const float Length = (Place == 1 ? 1600.f : (Place == 2 ? 950.f : 560.f)) * Grow;
			const float Thick = Place == 1 ? 130.f : (Place == 2 ? 90.f : 60.f);
			const int32 NumRays = Place == 1 ? 14 : 10;
			for (int32 i = 0; i < NumRays; ++i)
			{
				const float Angle = Time * (Place == 1 ? 0.35f : 0.25f) + PI * static_cast<float>(i) / NumRays;
				const FVector2f RaySize(Length, Thick);
				FSlateDrawElement::MakeRotatedBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(RaySize, FSlateLayoutTransform(Center - RaySize * 0.5f)),
					&SolidBrush, ESlateDrawEffect::None, Angle, TOptional<FVector2f>(), FSlateDrawElement::RelativeToElement,
					FLinearColor(Ray.R, Ray.G, Ray.B, (i % 2) ? 0.07f : 0.13f) * Tint);
			}
			const float GlowSize = (Place == 1 ? 860.f : (Place == 2 ? 580.f : 360.f)) * Grow;
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
				AllottedGeometry.ToPaintGeometry(FVector2f(GlowSize, GlowSize), FSlateLayoutTransform(Center - FVector2f(GlowSize, GlowSize) * 0.5f)),
				&GlowBrush, ESlateDrawEffect::None, FLinearColor(Ray.R, Ray.G, Ray.B, 0.55f) * Tint);
		}
		else
		{
			const float GlowSize = 520.f * Grow;
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
				AllottedGeometry.ToPaintGeometry(FVector2f(GlowSize, GlowSize), FSlateLayoutTransform(Center - FVector2f(GlowSize, GlowSize) * 0.5f)),
				&GlowBrush, ESlateDrawEffect::None, FLinearColor(0.45f, 0.5f, 0.58f, 0.28f) * Tint);
		}
	}

	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 2, InWidgetStyle, bParentEnabled);
	if (!bPlaying)
	{
		return Layer;
	}

	// La nubecita gris que les llueve encima (del 6.º en adelante).
	if (Place >= 6 && SinceLand > 0.f)
	{
		const float In = TNRaceUI::Smooth(SinceLand / 0.3f);
		const FVector2f CloudSize(260.f * In, 130.f * In);
		const FVector2f CloudCenter(Center.X + 8.f * FMath::Sin(Time * 1.3f), Center.Y - 0.5f * static_cast<float>(PrizeExtent.Y) - 95.f);
		FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1, AllottedGeometry.ToPaintGeometry(CloudSize, FSlateLayoutTransform(CloudCenter - CloudSize * 0.5f)),
			&CloudBrush, ESlateDrawEffect::None, FLinearColor(0.5f, 0.54f, 0.6f, 0.95f) * Tint);
	}

	// Confeti, destellos, granos y lluvia.
	for (const FTNArrivalParticle& Piece : Particles)
	{
		const FVector2f PieceSize(static_cast<float>(Piece.Size.X), static_cast<float>(Piece.Size.Y));
		const FVector2f TopLeft(static_cast<float>(Piece.Pos.X) - 0.5f * PieceSize.X, static_cast<float>(Piece.Pos.Y) - 0.5f * PieceSize.Y);
		const FPaintGeometry Geometry = AllottedGeometry.ToPaintGeometry(PieceSize, FSlateLayoutTransform(TopLeft));
		if (Piece.Kind == KindSparkle)
		{
			// Nace, brilla y se apaga.
			const float U = FMath::Clamp(Piece.Age / FMath::Max(0.05f, Piece.Life), 0.f, 1.f);
			const float Alpha = FMath::Sin(U * PI);
			FSlateDrawElement::MakeRotatedBox(OutDrawElements, Layer + 2, Geometry, &SparkBrush, ESlateDrawEffect::None, FMath::DegreesToRadians(Piece.Angle),
				TOptional<FVector2f>(), FSlateDrawElement::RelativeToElement, FLinearColor(Piece.Tint.R, Piece.Tint.G, Piece.Tint.B, Piece.Tint.A * Alpha) * Tint);
			continue;
		}
		const float Fade = Piece.Kind == KindRain ? 1.f : FMath::Clamp((Piece.Life - Piece.Age) / 0.3f, 0.f, 1.f);
		FSlateDrawElement::MakeRotatedBox(OutDrawElements, Layer + 2, Geometry, &SolidBrush, ESlateDrawEffect::None, FMath::DegreesToRadians(Piece.Angle),
			TOptional<FVector2f>(), FSlateDrawElement::RelativeToElement, FLinearColor(Piece.Tint.R, Piece.Tint.G, Piece.Tint.B, Piece.Tint.A * Fade) * Tint);
	}
	return Layer + 2;
}
