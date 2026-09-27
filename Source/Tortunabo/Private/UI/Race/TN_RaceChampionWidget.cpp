#include "UI/Race/TN_RaceChampionWidget.h"
#include "TN_RaceArt.h"
#include "TN_RaceUIKit.h"
#include "Audio/TN_ScoreShellSynthComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Game/TN_BeachRaceGameMode.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateTypes.h"
#include "UI/Race/TN_RacePodiumStage.h"
#include "World/TN_ScoreShells.h"

// Con nombre (no anónimo): en la compilación por bloques (unity) los nombres de un espacio anónimo se ven en el resto
// del bloque.
namespace TNRaceChampionDetail
{
	TAutoConsoleVariable<float> CVarPodiumExposure(TEXT("TN.Race.PodiumExposure"), 1.5f,
		TEXT("Podio del campeón: exposición de la captura en la pantalla (M_UI_Preview)."));

	/** Material de la interfaz que pinta una captura SceneColorHDR (el del escaparate de la tienda). */
	const TCHAR* const PreviewMaterialPath = TEXT("/Game/UI/Shop/M_UI_Preview.M_UI_Preview");

	/** Ancho del panel azul marino de la izquierda y de los botones (unidades de la interfaz a 1080 p). */
	constexpr float PanelWidth = 860.f;
	constexpr float ButtonW = 420.f;
	constexpr float ButtonH = 86.f;

	/** Colores de la medalla de cada puesto (los de los resultados del HUD). */
	FLinearColor MedalColor(int32 Place)
	{
		return Place == 0 ? TNHUDArt::Gold : (Place == 1 ? TNHUDArt::Hex(0xDDE6EE) : TNHUDArt::Hex(0xE8A56B));
	}

	/** Colores del confeti. */
	FLinearColor ConfettiTint(int32 Index)
	{
		static const uint32 Hexes[] = { 0xFF6A52, 0xFFCB3D, 0x1E9CC6, 0xFF8FD8, 0x59C96B, 0xFFFBF0 };
		return TNHUDArt::Hex(Hexes[Index % 6]);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Construcción
// ─────────────────────────────────────────────────────────────────────────────

void UTN_RaceChampionWidget::NativeOnInitialized()
{
	BuildTree();
	Super::NativeOnInitialized();
}

void UTN_RaceChampionWidget::BuildTree()
{
	using namespace TNRaceChampionDetail;
	if (!WidgetTree || Canvas) { return; }
	UWidgetTree* Tree = WidgetTree;
	Canvas = TNRaceUI::Make<UCanvasPanel>(Tree, TEXT("RaceChampionCanvas"));
	Tree->RootWidget = Canvas;

	// Cielo pintado (la captura del podio es transparente por encima del mar), sol y nubes que pasan.
	TNRaceUI::Fill(Canvas, TNRaceUI::MakeImage(Tree, TNRaceArt::SkyGradient(), FVector2D(8.f, 256.f)));
	SunImage = TNRaceUI::MakeImage(Tree, TNRaceArt::SunGlow(), FVector2D(380.f, 380.f));
	SunImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	TNRaceUI::Place(Canvas, SunImage, FVector2D(1.f, 0.f), FVector2D(-170.f, -20.f));
	const FVector2D CloudSizes[3] = { FVector2D(330.f, 165.f), FVector2D(240.f, 120.f), FVector2D(400.f, 200.f) };
	for (const FVector2D& CloudSize : CloudSizes)
	{
		UImage* Cloud = TNRaceUI::MakeImage(Tree, TNRaceArt::Cloud(), CloudSize);
		Cloud->SetRenderOpacity(0.92f);
		TNRaceUI::PlaceAt(Canvas, Cloud, FVector2D::ZeroVector, CloudSize, FVector2D(0.f, 0.f));
		Clouds.Add(Cloud);
	}

	// El podio: la captura de ATN_RacePodiumStage (se encaja a pantalla completa cada fotograma).
	PodiumImage = TNRaceUI::Make<UImage>(Tree);
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, PreviewMaterialPath))
	{
		PodiumMID = UMaterialInstanceDynamic::Create(Base, this);
		PodiumImage->SetBrushFromMaterial(PodiumMID);
	}
	PodiumImage->SetRenderTransformPivot(FVector2D(0.62f, 0.5f));
	TNRaceUI::PlaceAt(Canvas, PodiumImage, FVector2D::ZeroVector, FVector2D(1920.f, 1080.f), FVector2D(0.f, 0.f));

	// Panel azul marino a la izquierda, que se funde con el fondo hacia la derecha.
	{
		UImage* Panel = TNRaceUI::MakeImage(Tree, TNRaceArt::SidePanel(), FVector2D(256.f, 8.f));
		UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
		PanelSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 1.f));
		PanelSlot->SetOffsets(FMargin(0.f, 0.f, PanelWidth, 0.f));
	}

	// Nombre de cada tortuga encima de su cabeza en el podio.
	for (int32 i = 0; i < 3; ++i)
	{
		UTextBlock* TagText = TNRaceUI::MakeText(Tree, FText::GetEmpty(), TEXT("Bold"), 20, MedalColor(i));
		UBorder* Tag = TNRaceUI::Make<UBorder>(Tree);
		TNHUDStyle::StylePanel(Tag, TNHUDArt::Hex(0x0A1C38, 0.82f), 14.f, FMargin(16.f, 5.f, 18.f, 7.f), MedalColor(i), 2.f);
		Tag->SetContent(TagText);
		Tag->SetVisibility(ESlateVisibility::Collapsed);
		Tag->SetRenderTransformPivot(FVector2D(0.5f, 1.f));
		UCanvasPanelSlot* TagSlot = TNRaceUI::Place(Canvas, Tag, FVector2D(0.f, 0.f), FVector2D::ZeroVector);
		TagSlot->SetAlignment(FVector2D(0.5f, 1.f));
		NameTags.Add(Tag);
		NameTagTexts.Add(TagText);
	}

	// Columna de la izquierda: cinta, cartel de la campeona, botones y avisos.
	UVerticalBox* Column = TNRaceUI::Make<UVerticalBox>(Tree);
	{
		UBorder* Title = TNRaceUI::MakeCard(Tree, TNHUDArt::RibbonTexture(), TNRaceUI::RibbonMargin,
			TNRaceUI::MakeText(Tree, NSLOCTEXT("TNRace", "ChampionTitle", "¡CAMPEONA DE LA PLAYA!"), TEXT("Black"), 32, FLinearColor::White),
			FMargin(60.f, 20.f, 60.f, 22.f));
		Title->SetRenderTransformAngle(-2.f);
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Title)) { S->SetHorizontalAlignment(HAlign_Left); }
	}
	{
		UHorizontalBox* Row = TNRaceUI::Make<UHorizontalBox>(Tree);
		UOverlay* Portrait = TNRaceUI::Make<UOverlay>(Tree);
		ChampionFace = TNRaceUI::MakeImage(Tree, TNHUDFaces::TurtleFace(ETNTurtleFace::Win), FVector2D(160.f, 160.f));
		ChampionFace->SetRenderTransformPivot(FVector2D(0.5f, 0.85f));
		TNRaceUI::AddAt(Portrait, ChampionFace, HAlign_Center, VAlign_Bottom);
		ChampionCrown = TNRaceUI::MakeImage(Tree, TNRaceArt::Crown(), FVector2D(96.f, 72.f));
		ChampionCrown->SetRenderTransformPivot(FVector2D(0.5f, 1.f));
		TNRaceUI::AddAt(Portrait, ChampionCrown, HAlign_Center, VAlign_Top, FMargin(0.f, -40.f, 0.f, 0.f));
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(TNRaceUI::MakeSize(Tree, Portrait, 170.f, 190.f)))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, -10.f, 18.f, -10.f));
		}
		UVerticalBox* Info = TNRaceUI::Make<UVerticalBox>(Tree);
		ChampionName = TNRaceUI::MakeText(Tree, FText::GetEmpty(), TEXT("Bold"), 38, TNHUDArt::Gold);
		Info->AddChildToVerticalBox(ChampionName);
		UHorizontalBox* ShellRow = TNRaceUI::Make<UHorizontalBox>(Tree);
		for (int32 k = 0; k < 5; ++k)
		{
			UImage* Shell = TNRaceUI::MakeImage(Tree, TNHUDArt::ShellIconTier(3), FVector2D(54.f, 54.f));
			Shell->SetRenderTransformPivot(FVector2D(0.5f, 0.6f));
			Shell->SetVisibility(ESlateVisibility::Collapsed);
			if (UHorizontalBoxSlot* S = ShellRow->AddChildToHorizontalBox(Shell)) { S->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f)); }
			ChampionShells.Add(Shell);
		}
		if (UVerticalBoxSlot* S = Info->AddChildToVerticalBox(ShellRow)) { S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f)); }
		UTextBlock* Subtitle = TNRaceUI::MakeText(Tree, NSLOCTEXT("TNRace", "ChampionSubtitle", "¡Se lleva la partida!"), TEXT("Regular"), 19, TNHUDArt::Foam);
		if (UVerticalBoxSlot* S = Info->AddChildToVerticalBox(Subtitle)) { S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f)); }
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Info)) { S->SetVerticalAlignment(VAlign_Center); }
		UBorder* Card = TNRaceUI::MakeCard(Tree, TNHUDArt::CardTexture(), TNRaceUI::CardMargin, Row, FMargin(30.f, 34.f, 44.f, 46.f));
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Card)) { S->SetHorizontalAlignment(HAlign_Left); S->SetPadding(FMargin(0.f, 18.f, 0.f, 22.f)); }
	}
	PlayAgainButton = MakeButton(NSLOCTEXT("TNRace", "PlayAgain", "Volver a jugar"), static_cast<uint8>(TNRaceArt::EButtonIcon::Replay));
	ChangeModeButton = MakeButton(NSLOCTEXT("TNRace", "ChangeMode", "Cambiar de modo"), static_cast<uint8>(TNRaceArt::EButtonIcon::Modes));
	QuitButton = MakeButton(NSLOCTEXT("TNRace", "Quit", "Salir"), static_cast<uint8>(TNRaceArt::EButtonIcon::Exit));
	PlayAgainButton->OnClicked.AddDynamic(this, &UTN_RaceChampionWidget::HandlePlayAgain);
	ChangeModeButton->OnClicked.AddDynamic(this, &UTN_RaceChampionWidget::HandleChangeMode);
	QuitButton->OnClicked.AddDynamic(this, &UTN_RaceChampionWidget::HandleQuit);
	for (UButton* Button : { PlayAgainButton.Get(), ChangeModeButton.Get(), QuitButton.Get() })
	{
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Button)) { S->SetHorizontalAlignment(HAlign_Left); S->SetPadding(FMargin(8.f, 0.f, 0.f, 14.f)); }
	}
	{
		UHorizontalBox* Note = TNRaceUI::Make<UHorizontalBox>(Tree);
		UImage* Lock = TNRaceUI::MakeImage(Tree, TNRaceArt::ButtonIcon(TNRaceArt::EButtonIcon::Lock), FVector2D(30.f, 30.f));
		Lock->SetColorAndOpacity(TNHUDArt::SandC);
		if (UHorizontalBoxSlot* S = Note->AddChildToHorizontalBox(Lock)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f)); }
		UTextBlock* NoteText = TNRaceUI::MakeText(Tree, NSLOCTEXT("TNRace", "HostDecides", "Volver a jugar o cambiar de modo lo decide el anfitrión."),
			TEXT("Regular"), 17, TNHUDArt::Foam);
		if (UHorizontalBoxSlot* S = Note->AddChildToHorizontalBox(NoteText)) { S->SetVerticalAlignment(VAlign_Center); }
		Note->SetVisibility(ESlateVisibility::Collapsed);
		HostNote = Note;
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Note)) { S->SetPadding(FMargin(12.f, 0.f, 0.f, 6.f)); }
	}
	StatusText = TNRaceUI::MakeText(Tree, FText::GetEmpty(), TEXT("Bold"), 20, TNHUDArt::SandC);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(StatusText)) { S->SetPadding(FMargin(12.f, 2.f, 0.f, 0.f)); }
	LeftColumn = Column;
	UCanvasPanelSlot* ColumnSlot = TNRaceUI::Place(Canvas, Column, FVector2D(0.f, 0.5f), FVector2D(80.f, 10.f));
	ColumnSlot->SetAlignment(FVector2D(0.f, 0.5f));

	// Vista previa por consola.
	UBorder* Preview = TNRaceUI::MakeCard(Tree, TNHUDArt::RibbonTexture(), TNRaceUI::RibbonMargin,
		TNRaceUI::MakeText(Tree, NSLOCTEXT("TNRace", "Preview", "VISTA PREVIA"), TEXT("Bold"), 15, FLinearColor::White), FMargin(34.f, 14.f, 34.f, 16.f));
	Preview->SetVisibility(ESlateVisibility::Collapsed);
	PreviewTag = Preview;
	TNRaceUI::Place(Canvas, Preview, FVector2D(1.f, 1.f), FVector2D(-24.f, -24.f));

	ConfettiBrush = FSlateColorBrush(FLinearColor::White);
}

UButton* UTN_RaceChampionWidget::MakeButton(const FText& Label, uint8 Icon)
{
	using namespace TNRaceChampionDetail;
	UWidgetTree* Tree = WidgetTree;
	UButton* Button = TNRaceUI::Make<UButton>(Tree);
	UTexture2D* Tag = TNHUDArt::SandTagTexture();
	FButtonStyle Style;
	Style.SetNormal(TNRaceUI::BoxBrush(Tag, TNRaceUI::TagMargin));
	Style.SetHovered(TNRaceUI::BoxBrush(Tag, TNRaceUI::TagMargin, TNHUDArt::Hex(0xFFE08A)));
	Style.SetPressed(TNRaceUI::BoxBrush(Tag, TNRaceUI::TagMargin, TNHUDArt::Hex(0xEBC27A)));
	Style.SetDisabled(TNRaceUI::BoxBrush(Tag, TNRaceUI::TagMargin, FLinearColor(0.5f, 0.53f, 0.6f, 0.6f)));
	Style.SetNormalPadding(FMargin(0.f));
	Style.SetPressedPadding(FMargin(0.f, 3.f, 0.f, 0.f));
	Button->SetStyle(Style);
	Button->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));

	UHorizontalBox* Row = TNRaceUI::Make<UHorizontalBox>(Tree);
	UImage* IconImage = TNRaceUI::MakeImage(Tree, TNRaceArt::ButtonIcon(static_cast<TNRaceArt::EButtonIcon>(Icon)), FVector2D(46.f, 46.f));
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(IconImage)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(38.f, 0.f, 16.f, 0.f)); }
	UTextBlock* LabelText = TNRaceUI::MakeText(Tree, Label, TEXT("Bold"), 27, TNHUDArt::Ink, false);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(LabelText)) { S->SetVerticalAlignment(VAlign_Center); }
	Button->SetContent(TNRaceUI::MakeSize(Tree, Row, ButtonW, ButtonH));
	Button->OnHovered.AddDynamic(this, &UTN_RaceChampionWidget::HandleHovered);
	return Button;
}

void UTN_RaceChampionWidget::Setup(const FTNRaceChampionSetup& InSetup)
{
	BuildTree();
	ChampionSetup = InSetup;
	ChampionSetup.Target = FMath::Clamp(ChampionSetup.Target, 1, ChampionShells.Num());
	Time = 0.f;
	bChoiceMade = false;
	Confetti.Reset();

	const FTNRaceTallyRow* First = ChampionSetup.Podium.IsValidIndex(0) ? &ChampionSetup.Podium[0] : nullptr;
	if (ChampionName) { ChampionName->SetText(FText::FromString(First && !First->Name.IsEmpty() ? First->Name : FString(TEXT("Tortuga")))); }
	if (ChampionFace && First) { TNRaceUI::SetImageTexture(ChampionFace, TNRaceArt::TurtleFaceFor(this, First->Look, ETNTurtleFace::Win)); }
	for (int32 k = 0; k < ChampionShells.Num(); ++k)
	{
		ChampionShells[k]->SetVisibility(k < ChampionSetup.Target ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		ChampionShells[k]->SetRenderScale(FVector2D(0.01f, 0.01f));
	}
	for (int32 i = 0; i < NameTagTexts.Num(); ++i)
	{
		if (ChampionSetup.Podium.IsValidIndex(i))
		{
			NameTagTexts[i]->SetText(FText::Format(NSLOCTEXT("TNRace", "PodiumPlace", "{0}.º {1}"), FText::AsNumber(i + 1),
				FText::FromString(ChampionSetup.Podium[i].Name.IsEmpty() ? FString(TEXT("Tortuga")) : ChampionSetup.Podium[i].Name)));
		}
	}
	if (StatusText)
	{
		StatusText->SetText(ChampionSetup.bPreview ? NSLOCTEXT("TNRace", "PreviewHint", "Vista previa: cualquier botón la cierra.") : FText::GetEmpty());
	}
	if (PreviewTag) { PreviewTag->SetVisibility(ChampionSetup.bPreview ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }

	// El podio de verdad: las tortugas con su aspecto y su pose, capturado cada fotograma mientras se vea esta pantalla.
	Stage = ATN_RacePodiumStage::Get(GetWorld());
	if (ATN_RacePodiumStage* PodiumStage = Stage.Get())
	{
		TArray<FTN_TurtleLook> Looks;
		for (const FTNRaceTallyRow& Row : ChampionSetup.Podium) { Looks.Add(Row.Look); }
		PodiumStage->SetPodium(Looks);
		PodiumStage->SetLive(true);
		if (PodiumMID) { PodiumMID->SetTextureParameterValue(TEXT("Capture"), PodiumStage->GetRenderTarget()); }
	}
	RefreshButtons();
}

void UTN_RaceChampionWidget::SetSecondsLeft(float InSeconds)
{
	SecondsLeft = FMath::Max(0.f, InSeconds);
}

void UTN_RaceChampionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Ratón a la vista para los botones (sin quitarle el teclado al juego).
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
		bInputTaken = true;
	}
}

void UTN_RaceChampionWidget::NativeDestruct()
{
	if (ATN_RacePodiumStage* PodiumStage = Stage.Get()) { PodiumStage->SetLive(false); }
	if (bInputTaken)
	{
		bInputTaken = false;
		if (APlayerController* PC = GetOwningPlayer())
		{
			PC->SetInputMode(FInputModeGameOnly());
			PC->SetShowMouseCursor(false);
		}
	}
	Super::NativeDestruct();
}

// ─────────────────────────────────────────────────────────────────────────────
// Animación
// ─────────────────────────────────────────────────────────────────────────────

void UTN_RaceChampionWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	using namespace TNRaceChampionDetail;
	Super::NativeTick(MyGeometry, InDeltaTime);
	const float Dt = FMath::Min(InDeltaTime, 0.1f);
	const float Before = Time;
	Time += Dt;
	const FVector2f Size = FVector2f(MyGeometry.GetLocalSize());
	if (Size.X > 1.f) { LocalSize = FVector2D(Size.X, Size.Y); }

	// Entrada: todo aparece con un fundido, la columna llega desde la izquierda y el podio se acerca un poco.
	SetRenderOpacity(TNRaceUI::Smooth(Time / 0.5f));
	if (LeftColumn) { LeftColumn->SetRenderTranslation(FVector2D(-160.f * (1.f - TNRaceUI::PopIn(Time / 0.6f)), 0.f)); }
	if (PodiumImage)
	{
		const float Zoom = 1.06f - 0.06f * TNRaceUI::Smooth(Time / 1.4f);
		PodiumImage->SetRenderScale(FVector2D(Zoom, Zoom));
	}
	if (PodiumMID) { PodiumMID->SetScalarParameterValue(TEXT("Exposure"), CVarPodiumExposure.GetValueOnGameThread()); }

	// Cielo: el sol late despacio y las nubes pasan de derecha a izquierda.
	if (SunImage) { SunImage->SetRenderScale(FVector2D(1.f + 0.035f * FMath::Sin(Time * 1.3f))); }
	for (int32 i = 0; i < Clouds.Num(); ++i)
	{
		const float Span = static_cast<float>(LocalSize.X) + 520.f;
		const float X = static_cast<float>(LocalSize.X) + 60.f - FMath::Frac(Time * (0.012f + 0.006f * i) + 0.37f * i) * Span;
		const float Y = static_cast<float>(LocalSize.Y) * (0.07f + 0.1f * i) + 8.f * FMath::Sin(Time * 0.4f + i);
		Clouds[i]->SetRenderTranslation(FVector2D(X, Y));
	}

	// La campeona: su cara salta, la corona se mece y sus conchas aparecen una a una con un «pom» que sube.
	if (ChampionFace)
	{
		const float Jump = FMath::Abs(FMath::Sin(Time * 4.f));
		ChampionFace->SetRenderTranslation(FVector2D(0.f, -10.f * Jump));
		ChampionFace->SetRenderTransformAngle(6.f * FMath::Sin(Time * 3.f));
	}
	if (ChampionCrown)
	{
		const float Jump = FMath::Abs(FMath::Sin(Time * 4.f));
		ChampionCrown->SetRenderTranslation(FVector2D(0.f, -10.f * Jump - 4.f * FMath::Abs(FMath::Sin(Time * 4.f + 0.4f))));
		ChampionCrown->SetRenderTransformAngle(-10.f + 5.f * FMath::Sin(Time * 3.f + 0.5f));
	}
	for (int32 k = 0; k < ChampionSetup.Target && ChampionShells.IsValidIndex(k); ++k)
	{
		const float At = 0.55f + 0.18f * k;
		if (Before < At && Time >= At) { PlaySound(false, 1, static_cast<float>(TNScoreShells::PomSemitones(k * 2)), 0.8f); }
		const float Pop = FMath::Max(0.01f, TNRaceUI::PopIn((Time - At) / 0.3f));
		ChampionShells[k]->SetRenderScale(FVector2D(Pop, Pop));
		ChampionShells[k]->SetRenderTransformAngle(8.f * FMath::Sin(Time * 2.5f + k));
	}

	// Botones: se inflan un poco bajo el ratón; quién puede elegir se mira de vez en cuando (llega por red).
	if (FMath::FloorToInt(Before * 2.f) != FMath::FloorToInt(Time * 2.f)) { RefreshButtons(); }
	for (UButton* Button : { PlayAgainButton.Get(), ChangeModeButton.Get(), QuitButton.Get() })
	{
		if (!Button) { continue; }
		const float Want = (Button->IsHovered() && Button->GetIsEnabled()) ? 1.05f + 0.01f * FMath::Sin(Time * 8.f) : 1.f;
		const float Now = static_cast<float>(Button->GetRenderTransform().Scale.X);
		const float Next = FMath::FInterpTo(Now, Want, Dt, 14.f);
		Button->SetRenderScale(FVector2D(Next, Next));
	}
	if (StatusText && !bChoiceMade && !ChampionSetup.bPreview)
	{
		const int32 Seconds = FMath::CeilToInt(SecondsLeft);
		StatusText->SetText(Seconds > 0 ? FText::Format(NSLOCTEXT("TNRace", "ChampionSeconds", "Quedan {0} s"), FText::AsNumber(Seconds)) : FText::GetEmpty());
	}

	TickPodiumImage();
	TickConfetti(Dt);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UTN_RaceChampionWidget::TickPodiumImage()
{
	// La captura (16:9) cubre toda la pantalla sin deformarse: se recorta lo que sobre por arriba y abajo o a los lados.
	UCanvasPanelSlot* ImageSlot = PodiumImage ? Cast<UCanvasPanelSlot>(PodiumImage->Slot) : nullptr;
	if (!ImageSlot) { return; }
	const double Aspect = ATN_RacePodiumStage::CaptureAspect;
	double W = LocalSize.X;
	double H = W / Aspect;
	if (H < LocalSize.Y)
	{
		H = LocalSize.Y;
		W = H * Aspect;
	}
	const FVector2D ImageSize(W, H);
	const FVector2D ImagePos = (LocalSize - ImageSize) * 0.5;
	ImageSlot->SetPosition(ImagePos);
	ImageSlot->SetSize(ImageSize);

	// Nombres encima de las cabezas, con el mismo acercamiento que la imagen.
	const double Zoom = PodiumImage->GetRenderTransform().Scale.X;
	const FVector2D Pivot = ImagePos + FVector2D(0.62, 0.5) * ImageSize;
	const ATN_RacePodiumStage* PodiumStage = Stage.Get();
	for (int32 i = 0; i < NameTags.Num(); ++i)
	{
		UWidget* Tag = NameTags[i];
		FVector2D UV = FVector2D::ZeroVector;
		const bool bShow = PodiumStage && ChampionSetup.Podium.IsValidIndex(i) && PodiumStage->GetNameTagUV(i, UV) && Time > 0.8f;
		Tag->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (!bShow) { continue; }
		const FVector2D OnImage = ImagePos + UV * ImageSize;
		const FVector2D OnScreen = Pivot + (OnImage - Pivot) * Zoom;
		if (UCanvasPanelSlot* TagSlot = Cast<UCanvasPanelSlot>(Tag->Slot))
		{
			TagSlot->SetPosition(OnScreen + FVector2D(0.f, -4.f * FMath::Sin(Time * 2.f + i)));
		}
		const float Pop = TNRaceUI::PopIn((Time - 0.8f - 0.15f * i) / 0.35f);
		Tag->SetRenderScale(FVector2D(FMath::Max(0.01f, Pop), FMath::Max(0.01f, Pop)));
	}
}

void UTN_RaceChampionWidget::TickConfetti(float DeltaTime)
{
	using namespace TNRaceChampionDetail;
	const float W = static_cast<float>(LocalSize.X);
	const float H = static_cast<float>(LocalSize.Y);
	// Mucho al principio y luego una lluvia suave, solo sobre el podio (los botones quedan limpios).
	ConfettiDebt += DeltaTime * (Time < 3.f ? 45.f : 12.f);
	while (ConfettiDebt >= 1.f && Confetti.Num() < 180)
	{
		ConfettiDebt -= 1.f;
		FTNConfetti Piece;
		Piece.Pos = FVector2D(FMath::FRandRange(W * 0.4f, W), -20.f);
		Piece.Vel = FVector2D(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(110.f, 230.f));
		Piece.Size = FVector2D(FMath::FRandRange(8.f, 12.f), FMath::FRandRange(12.f, 19.f));
		Piece.Angle = FMath::FRandRange(0.f, 180.f);
		Piece.Spin = FMath::FRandRange(-260.f, 260.f);
		Piece.Sway = FMath::FRandRange(0.f, 2.f * PI);
		Piece.Tint = ConfettiTint(FMath::RandRange(0, 5));
		Confetti.Add(Piece);
	}
	ConfettiDebt = FMath::Min(ConfettiDebt, 1.f);
	for (int32 i = Confetti.Num() - 1; i >= 0; --i)
	{
		FTNConfetti& Piece = Confetti[i];
		Piece.Pos += Piece.Vel * DeltaTime;
		Piece.Pos.X += 26.f * FMath::Sin(Time * 2.2f + Piece.Sway) * DeltaTime;
		Piece.Angle += Piece.Spin * DeltaTime;
		if (Piece.Pos.Y > H + 30.f) { Confetti.RemoveAtSwap(i); }
	}
}

int32 UTN_RaceChampionWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
	for (const FTNConfetti& Piece : Confetti)
	{
		// Al girar, el papelito se ve de canto a ratos (se estrecha).
		const float Flip = 0.35f + 0.65f * FMath::Abs(FMath::Cos(FMath::DegreesToRadians(Piece.Angle * 1.7f)));
		const FVector2f PieceSize(static_cast<float>(Piece.Size.X) * Flip, static_cast<float>(Piece.Size.Y));
		FSlateDrawElement::MakeRotatedBox(OutDrawElements, Layer + 1, AllottedGeometry.ToPaintGeometry(PieceSize,
			FSlateLayoutTransform(FVector2f(static_cast<float>(Piece.Pos.X) - 0.5f * PieceSize.X, static_cast<float>(Piece.Pos.Y) - 0.5f * PieceSize.Y))),
			&ConfettiBrush, ESlateDrawEffect::None, FMath::DegreesToRadians(Piece.Angle), TOptional<FVector2f>(), FSlateDrawElement::RelativeToElement,
			Piece.Tint * Tint);
	}
	return Layer + 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Botones
// ─────────────────────────────────────────────────────────────────────────────

void UTN_RaceChampionWidget::RefreshButtons()
{
	bCanChoose = ChampionSetup.bPreview || ATN_BeachRaceGameMode::CanLocalPlayerChoose(this);
	if (PlayAgainButton) { PlayAgainButton->SetIsEnabled(bCanChoose && !bChoiceMade); }
	if (ChangeModeButton) { ChangeModeButton->SetIsEnabled(bCanChoose && !bChoiceMade); }
	if (QuitButton) { QuitButton->SetIsEnabled(!bChoiceMade); }
	if (HostNote) { HostNote->SetVisibility(bCanChoose ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }
	for (UButton* Button : { PlayAgainButton.Get(), ChangeModeButton.Get(), QuitButton.Get() })
	{
		if (Button) { Button->SetRenderOpacity(Button->GetIsEnabled() ? 1.f : 0.6f); }
	}
}

void UTN_RaceChampionWidget::HandlePlayAgain()
{
	Choose(static_cast<uint8>(ETNBeachChampionChoice::PlayAgain));
}

void UTN_RaceChampionWidget::HandleChangeMode()
{
	Choose(static_cast<uint8>(ETNBeachChampionChoice::ChangeMode));
}

void UTN_RaceChampionWidget::HandleQuit()
{
	Choose(static_cast<uint8>(ETNBeachChampionChoice::Quit));
}

void UTN_RaceChampionWidget::HandleHovered()
{
	PlaySound(false, 0, 7.f, 0.35f);
}

void UTN_RaceChampionWidget::Choose(uint8 Choice)
{
	if (bChoiceMade) { return; }
	PlaySound(true, 1, 0.f, 0.8f);
	if (ChampionSetup.bPreview)
	{
		bChoiceMade = true;
		if (OnPreviewClosed) { OnPreviewClosed(); }
		return;
	}
	// El flujo lo decide el GameMode: la interfaz solo pide (en un cliente solo vale Salir, que se va él solo).
	const ETNBeachChampionChoice Wanted = static_cast<ETNBeachChampionChoice>(Choice);
	if (ATN_BeachRaceGameMode::RequestChampionChoice(this, Wanted))
	{
		bChoiceMade = true;
		if (StatusText)
		{
			StatusText->SetText(Wanted == ETNBeachChampionChoice::Quit ? NSLOCTEXT("TNRace", "Bye", "¡Hasta la próxima!") : NSLOCTEXT("TNRace", "Going", "¡Allá vamos!"));
		}
	}
	else if (StatusText)
	{
		StatusText->SetText(NSLOCTEXT("TNRace", "HostOnly", "Eso lo decide el anfitrión."));
	}
	RefreshButtons();
}

void UTN_RaceChampionWidget::PlaySound(bool bPlin, uint8 Tier, float Semitones, float Volume)
{
	if (!Synth.IsValid()) { Synth = UTN_ScoreShellSynthComponent::Attach2D(GetOwningPlayer()); }
	if (UTN_ScoreShellSynthComponent* Comp = Synth.Get())
	{
		Comp->TriggerSound(bPlin ? ETNScoreShellSound::Plin : ETNScoreShellSound::Pom, Tier, Semitones, Volume);
	}
}
