#include "UI/HUD/TN_RunHUDWidget.h"
#include "TN_HUDStyle.h"
#include "TN_HUDArt.h"
#include "TN_HUDFaces.h"
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
#include "Core/TN_CoopGameState.h"
#include "Core/TN_CoopPlayerState.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/TN_ShellComponent.h"
#include "Player/TN_StaminaComponent.h"
#include "Player/TortugaCharacter.h"
#include "Voice/ProximityVoiceComponent.h"
#include "World/ProcMap/TN_PathStorm.h"
#include "World/ProcMap/TN_ProcMapGenerator.h"

namespace
{
	using namespace TNHUDStyle;

	/** Vista previa de los estados del distintivo (para probar y para el equipo de arte). */
	TAutoConsoleVariable<float> CVarHUDEnergy(TEXT("tn.HUD.Energy"), -1.f, TEXT("HUD: fuerza la energía del salvavidas (0-1); -1 = la real."));
	TAutoConsoleVariable<int32> CVarHUDFace(TEXT("tn.HUD.Face"), -1,
		TEXT("HUD: fuerza la cara (0 feliz, 1 cansada, 2 jadeando, 3 caparazón, 4 mareada, 5 victoria); -1 = la real."));
	TAutoConsoleVariable<int32> CVarHUDTalk(TEXT("tn.HUD.Talk"), -1, TEXT("HUD: 1 fuerza el bocadillo de voz, 0 lo apaga; -1 = el real."));
	TAutoConsoleVariable<int32> CVarHUDCrew(TEXT("tn.HUD.CrewPreview"), 0,
		TEXT("HUD: rellena N filas de la tripulación con tu propia tortuga (la 1.ª dice una frase y la 2.ª habla) para ver el diseño sin más jugadores."));

	/** Pista de la playa al mar: tamaño y tramo útil (del nido a la orilla), en fracción de su ancho. */
	constexpr float TrackW = 520.f;
	constexpr float TrackH = 66.f;
	constexpr float TrackFrom = 0.1f;
	constexpr float TrackTo = 0.84f;

	/** Inventario: burbujas iguales en columnas de ancho fijo (el aro de cuerda rueda de una a otra). */
	constexpr float BubbleSize = 90.f;
	constexpr float SlotColumn = 162.f;
	constexpr float SlotGap = 8.f;

	/** Rueda radial: tamaño y radio al que van las opciones (mitad del anillo de M_UI_RadialWheel). */
	constexpr float WheelSize = 500.f;
	constexpr float WheelSlotRadius = 0.3135f * WheelSize;

	/** Duración de un bocadillo de chat (s). */
	constexpr float BubbleLife = 4.5f;

	const FLinearColor NavyText = TNHUDArt::Ink;
	/** Color de cada compañero: su caparazón en la pista y el aro de su cara en la tripulación. */
	const FLinearColor MateColors[3] = { TNHUDArt::Hex(0x59C96B), TNHUDArt::Hex(0xFFC23D), TNHUDArt::Hex(0xB07CFF) };
	/** Márgenes de caja (fracción de la textura) de los carteles con arte de TNHUDArt. */
	const FMargin CardMargin(0.16f, 0.2f, 0.16f, 0.34f);
	const FMargin RibbonMargin(0.14f, 0.f, 0.14f, 0.f);
	const FMargin TagMargin(0.2f, 0.f, 0.2f, 0.f);
	const FMargin ChatBubbleMargin(0.26f, 0.3f, 0.18f, 0.45f);

	template <typename T>
	T* Make(UWidgetTree* Tree, const TCHAR* Name = nullptr)
	{
		return Tree->ConstructWidget<T>(T::StaticClass(), Name ? FName(Name) : NAME_None);
	}

	UTextBlock* MakeText(UWidgetTree* Tree, const TCHAR* Name, const FText& Content, FName Weight, int32 Size, const FLinearColor& Color, bool bOutline = true)
	{
		UTextBlock* T = Make<UTextBlock>(Tree, Name);
		T->SetText(Content);
		StyleText(T, Weight, Size, Color, bOutline);
		if (!bOutline) { T->SetShadowColorAndOpacity(FLinearColor::Transparent); }
		return T;
	}

	UImage* MakeImage(UWidgetTree* Tree, UTexture2D* Tex, const FVector2D& Size, const TCHAR* Name = nullptr)
	{
		UImage* I = Make<UImage>(Tree, Name);
		FSlateBrush B;
		B.SetResourceObject(Tex);
		B.ImageSize = Size;
		I->SetBrush(B);
		return I;
	}

	/** Cambia la textura de una imagen conservando su tamaño. */
	void SetImageTexture(UImage* I, UTexture2D* Tex)
	{
		if (!I) { return; }
		FSlateBrush B = I->GetBrush();
		if (B.GetResourceObject() == Tex) { return; }
		B.SetResourceObject(Tex);
		I->SetBrush(B);
	}

	/**
	 * Cartel con arte que se estira como caja (los bordes con dibujo quedan al tamaño real de la textura) y contenido
	 * con relleno. El contenido tiene que medir al menos lo que los bordes, o Slate los encoge y el dibujo deja de
	 * casar con el relleno.
	 */
	UBorder* MakeCard(UWidgetTree* Tree, UTexture2D* Tex, const FMargin& Margin, UWidget* Content, const FMargin& Padding)
	{
		FSlateBrush B;
		B.SetResourceObject(Tex);
		B.DrawAs = ESlateBrushDrawType::Box;
		B.Margin = Margin;
		if (Tex) { B.ImageSize = FVector2D(Tex->GetSizeX(), Tex->GetSizeY()); }
		UBorder* Card = Make<UBorder>(Tree);
		Card->SetBrush(B);
		Card->SetPadding(Padding);
		Card->SetHorizontalAlignment(HAlign_Center);
		Card->SetVerticalAlignment(VAlign_Center);
		if (Content) { Card->SetContent(Content); }
		return Card;
	}

	USizeBox* MakeSize(UWidgetTree* Tree, UWidget* Content, float W, float H)
	{
		USizeBox* S = Make<USizeBox>(Tree);
		if (W > 0.f) { S->SetWidthOverride(W); }
		if (H > 0.f) { S->SetHeightOverride(H); }
		if (Content) { S->SetContent(Content); }
		return S;
	}

	/** Añade a un Overlay con alineación y relleno (por defecto los hijos van arriba a la izquierda a su tamaño). */
	UOverlaySlot* AddAt(UOverlay* Parent, UWidget* W, EHorizontalAlignment H, EVerticalAlignment V, const FMargin& Padding = FMargin(0.f))
	{
		UOverlaySlot* Slot = Parent->AddChildToOverlay(W);
		if (Slot)
		{
			Slot->SetHorizontalAlignment(H);
			Slot->SetVerticalAlignment(V);
			Slot->SetPadding(Padding);
		}
		return Slot;
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

	UMaterialInstanceDynamic* MakeUIMID(UObject* Outer, const TCHAR* Path)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, Path);
		return Base ? UMaterialInstanceDynamic::Create(Base, Outer) : nullptr;
	}

	/** Cartel azul marino con la cara de la tortuga a la izquierda y el texto a la derecha. */
	UBorder* MakeFaceCard(UWidgetTree* Tree, ETNTurtleFace Face, float FaceSize, UWidget* Content)
	{
		UHorizontalBox* Row = Make<UHorizontalBox>(Tree);
		UImage* FaceImg = MakeImage(Tree, TNHUDFaces::TurtleFace(Face), FVector2D(FaceSize, FaceSize));
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(FaceImg)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, -12.f, 10.f, -12.f)); }
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Content)) { S->SetVerticalAlignment(VAlign_Center); }
		return MakeCard(Tree, TNHUDArt::CardTexture(), CardMargin, Row, FMargin(20.f, 14.f, 28.f, 36.f));
	}

	/** Bocadillo de voz de ancho Width con cuatro barras de volumen dentro (se animan con AnimateTalkBars). */
	UOverlay* MakeTalkBubble(UWidgetTree* Tree, float Width, TArray<TObjectPtr<UImage>>& OutBars)
	{
		const float Height = Width * 112.f / 128.f;
		UOverlay* Root = Make<UOverlay>(Tree);
		AddAt(Root, MakeImage(Tree, TNHUDArt::TalkBubbleTexture(), FVector2D(Width, Height)), HAlign_Fill, VAlign_Fill);
		UHorizontalBox* Bars = Make<UHorizontalBox>(Tree);
		for (int32 i = 0; i < 4; ++i)
		{
			UImage* BarImg = Make<UImage>(Tree);
			BarImg->SetBrush(Rounded((i % 2) ? TNHUDArt::Sea : TNHUDArt::Navy, 3.f));
			BarImg->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			if (UHorizontalBoxSlot* S = Bars->AddChildToHorizontalBox(MakeSize(Tree, BarImg, Width * 0.075f, Height * 0.42f)))
			{
				S->SetPadding(FMargin(Width * 0.03f, 0.f));
				S->SetVerticalAlignment(VAlign_Center);
			}
			OutBars.Add(BarImg);
		}
		// Centradas en el cuerpo del bocadillo (algo a la derecha y arriba del centro de la textura, por la cola).
		AddAt(Root, Bars, HAlign_Center, VAlign_Center, FMargin(Width * 0.0625f, 0.f, 0.f, Height * 0.143f));
		return Root;
	}

	void AnimateTalkBars(const TArray<TObjectPtr<UImage>>& Bars, int32 From, float Time, float Seed)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			if (UImage* BarImg = Bars.IsValidIndex(From + i) ? Bars[From + i].Get() : nullptr)
			{
				const float Level = 0.25f + 0.75f * FMath::Abs(FMath::Sin(Time * (7.f + 2.3f * i) + Seed + i * 1.7f));
				BarImg->SetRenderScale(FVector2D(1.f, Level));
			}
		}
	}

	/** Compañeros (todos los PlayerState menos el tuyo) en el orden del GameState: fija su color y su fila. */
	TArray<const APlayerState*> CrewOf(const UWorld* World, const APlayerState* Own)
	{
		TArray<const APlayerState*> Out;
		if (const AGameStateBase* GS = World ? World->GetGameState() : nullptr)
		{
			for (const APlayerState* PS : GS->PlayerArray)
			{
				if (PS && PS != Own && !PS->IsInactive()) { Out.Add(PS); }
			}
		}
		return Out;
	}

	/** Bocadillo de chat con la frase dentro; el texto tiene un mínimo de tamaño para que el cuerpo siempre lo contenga. */
	UBorder* MakeChatBubble(UWidgetTree* Tree, int32 FontSize, float MaxWidth, UTextBlock*& OutText)
	{
		OutText = MakeText(Tree, nullptr, FText::GetEmpty(), TEXT("Bold"), FontSize, NavyText, false);
		OutText->SetAutoWrapText(true);
		OutText->SetJustification(ETextJustify::Center);
		USizeBox* Limit = MakeSize(Tree, OutText, 0.f, 0.f);
		Limit->SetMaxDesiredWidth(MaxWidth);
		Limit->SetMinDesiredWidth(34.f);
		Limit->SetMinDesiredHeight(22.f);
		UBorder* Bubble = MakeCard(Tree, TNHUDArt::ChatBubbleTexture(), ChatBubbleMargin, Limit, FMargin(22.f, 11.f, 14.f, 20.f));
		Bubble->SetRenderTransformPivot(FVector2D(0.f, 1.f));
		Bubble->SetVisibility(ESlateVisibility::Collapsed);
		return Bubble;
	}

	/** Energía (0-1) y agotamiento de una tortuga por su componente de estamina (replicado a todos). */
	void EnergyOf(const APawn* Pawn, float& OutEnergy, bool& bOutExhausted)
	{
		OutEnergy = 1.f;
		bOutExhausted = false;
		if (const UTN_StaminaComponent* St = Pawn ? Pawn->FindComponentByClass<UTN_StaminaComponent>() : nullptr)
		{
			OutEnergy = FMath::Clamp(St->GetCurrentStamina() / FMath::Max(1.f, St->GetMaxStamina()), 0.f, 1.f);
			bOutExhausted = St->IsExhausted();
		}
	}

	/** Cara de una tortuga según su estado, con margen en los umbrales de energía para que no parpadee. */
	ETNTurtleFace FaceFor(const APlayerState* PS, const APawn* Pawn, float Energy, bool bExhausted, ETNTurtleFace Prev)
	{
		const ATN_CoopPlayerState* TNPS = Cast<ATN_CoopPlayerState>(PS);
		if (TNPS && TNPS->bHasFinishedRun && !TNPS->bIsEliminated) { return ETNTurtleFace::Win; }
		if (TNPS && (TNPS->bIsDBNO || TNPS->bIsEliminated)) { return ETNTurtleFace::Down; }
		const UTN_ShellComponent* ShellComp = Pawn ? Pawn->FindComponentByClass<UTN_ShellComponent>() : nullptr;
		if (ShellComp && ShellComp->IsInShell()) { return ETNTurtleFace::Shell; }
		const bool bWasPanting = Prev == ETNTurtleFace::Panting;
		const bool bWasTired = Prev == ETNTurtleFace::Tired || bWasPanting;
		if (bExhausted || Energy < (bWasPanting ? 0.3f : 0.22f)) { return ETNTurtleFace::Panting; }
		if (Energy < (bWasTired ? 0.6f : 0.5f)) { return ETNTurtleFace::Tired; }
		return ETNTurtleFace::Happy;
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

	// Todas las caras se dibujan ahora (al entrar en el mapa) para que cambiar de estado no dé tirones.
	for (int32 f = 0; f <= static_cast<int32>(ETNTurtleFace::Win); ++f) { TNHUDFaces::TurtleFace(static_cast<ETNTurtleFace>(f)); }

	// La clase base rellena estos widgets (estamina, peso, número e iconos del inventario): existen pero no se ven; el
	// Tick los lee para pintar el salvavidas y las burbujas.
	{
		UVerticalBox* Feed = Make<UVerticalBox>(Tree);
		StaminaBar = Make<UProgressBar>(Tree, TEXT("StaminaBar"));
		StaminaBar->SetPercent(1.f);
		WeightPenaltyBar = Make<UProgressBar>(Tree, TEXT("WeightPenaltyBar"));
		WeightPenaltyBar->SetPercent(0.f);
		StaminaText = MakeText(Tree, TEXT("StaminaText"), FText::GetEmpty(), TEXT("Regular"), 10, Text);
		SlotEquippedImage = Make<UImage>(Tree, TEXT("SlotEquippedImage"));
		SlotEquippedImage->SetColorAndOpacity(FLinearColor::Transparent);
		SlotStoredImage = Make<UImage>(Tree, TEXT("SlotStoredImage"));
		SlotStoredImage->SetColorAndOpacity(FLinearColor::Transparent);
		for (UWidget* W : { static_cast<UWidget*>(StaminaBar), static_cast<UWidget*>(WeightPenaltyBar), static_cast<UWidget*>(StaminaText),
			static_cast<UWidget*>(SlotEquippedImage), static_cast<UWidget*>(SlotStoredImage) })
		{
			Feed->AddChildToVerticalBox(W);
		}
		Feed->SetVisibility(ESlateVisibility::Collapsed);
		Place(Canvas, Feed, FVector2D(0.f, 0.f), FVector2D(0.f, 0.f));
	}

	// ── Distintivo (abajo a la izquierda): la cara en el salvavidas de energía y, debajo, la cinta con el nombre ──
	{
		UVerticalBox* Col = Make<UVerticalBox>(Tree);
		UOverlay* Ring = Make<UOverlay>(Tree);
		Badge = Make<UImage>(Tree, TEXT("TurtleBadge"));
		BadgeMID = MakeUIMID(this, TEXT("/Game/UI/HUD/M_UI_TurtleBadge.M_UI_TurtleBadge"));
		if (BadgeMID) { Badge->SetBrushFromMaterial(BadgeMID); }
		AddAt(Ring, MakeSize(Tree, Badge, 196.f, 196.f), HAlign_Center, VAlign_Center);
		FaceImage = MakeImage(Tree, TNHUDFaces::TurtleFace(ETNTurtleFace::Happy), FVector2D(104.f, 104.f));
		FaceImage->SetRenderTransformPivot(FVector2D(0.5f, 0.85f));
		AddAt(Ring, FaceImage, HAlign_Center, VAlign_Center);
		// Hablando por la voz de proximidad: bocadillo con barras de volumen arriba a la derecha.
		TalkBubble = MakeTalkBubble(Tree, 66.f, TalkBars);
		TalkBubble->SetRenderTransformPivot(FVector2D(0.1f, 0.95f));
		TalkBubble->SetVisibility(ESlateVisibility::Collapsed);
		AddAt(Ring, TalkBubble, HAlign_Right, VAlign_Top, FMargin(0.f, -18.f, -44.f, 0.f));
		// Sin aliento: etiqueta coral que late junto al salvavidas.
		UTextBlock* Tired = MakeText(Tree, nullptr, NSLOCTEXT("TNHUD", "Exhausted", "¡SIN ALIENTO!"), TEXT("Bold"), 13, FLinearColor::White);
		ExhaustedRoot = MakeCard(Tree, TNHUDArt::RibbonTexture(), RibbonMargin, Tired, FMargin(28.f, 16.f, 28.f, 18.f));
		ExhaustedRoot->SetVisibility(ESlateVisibility::Hidden);
		ExhaustedRoot->SetRenderTransformAngle(-8.f);
		AddAt(Ring, ExhaustedRoot, HAlign_Right, VAlign_Bottom, FMargin(0.f, 0.f, -86.f, 34.f));
		if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Ring)) { S->SetHorizontalAlignment(HAlign_Center); }

		NameText = MakeText(Tree, nullptr, FText::GetEmpty(), TEXT("Bold"), 16, FLinearColor::White);
		NameText->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(MakeCard(Tree, TNHUDArt::RibbonTexture(), RibbonMargin, NameText, FMargin(40.f, 17.f, 40.f, 19.f))))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, -8.f, 0.f, 0.f));
		}
		Place(Canvas, Col, FVector2D(0.f, 1.f), FVector2D(22.f, -8.f));
	}

	// ── Inventario (abajo en el centro): dos burbujas iguales; el aro de cuerda marca la que va en la aleta ──
	{
		UOverlay* Root = Make<UOverlay>(Tree);
		UHorizontalBox* Row = Make<UHorizontalBox>(Tree);
		for (int32 i = 0; i < 2; ++i)
		{
			UOverlay* Bubble = Make<UOverlay>(Tree);
			AddAt(Bubble, MakeImage(Tree, TNHUDArt::BubbleIcon(), FVector2D(BubbleSize, BubbleSize)), HAlign_Fill, VAlign_Fill);
			UImage* Item = Make<UImage>(Tree);
			Item->SetColorAndOpacity(FLinearColor::Transparent);
			AddAt(Bubble, Item, HAlign_Fill, VAlign_Fill, FMargin(19.f));
			ItemImages.Add(Item);
			UVerticalBox* Column = Make<UVerticalBox>(Tree);
			if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(MakeSize(Tree, Bubble, BubbleSize, BubbleSize))) { S->SetHorizontalAlignment(HAlign_Center); }
			UTextBlock* Tag = MakeText(Tree, nullptr, FText::GetEmpty(), TEXT("Bold"), 11, NavyText, false);
			SlotTags.Add(Tag);
			if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(MakeCard(Tree, TNHUDArt::SandTagTexture(), TagMargin, Tag, FMargin(18.f, 12.f, 18.f, 13.f))))
			{
				S->SetHorizontalAlignment(HAlign_Center);
				S->SetPadding(FMargin(0.f, -8.f, 0.f, 0.f));
			}
			if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(MakeSize(Tree, Column, SlotColumn, 0.f))) { S->SetPadding(FMargin(i == 0 ? 0.f : SlotGap, 0.f, 0.f, 0.f)); }
		}
		SlotTags[0]->SetText(NSLOCTEXT("TNHUD", "InHand", "EN LA ALETA"));
		SlotTags[1]->SetText(NSLOCTEXT("TNHUD", "Stored", "EN EL CAPARAZÓN"));
		AddAt(Root, Row, HAlign_Left, VAlign_Top);
		// El aro de cuerda va por encima y se traslada de una burbuja a la otra (TickInventory).
		RopeImage = MakeImage(Tree, TNHUDArt::RopeRing(), FVector2D(BubbleSize + 10.f, BubbleSize + 10.f), TEXT("SlotEquippedSelector"));
		RopeImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		SlotEquippedSelector = RopeImage;
		AddAt(Root, RopeImage, HAlign_Left, VAlign_Top, FMargin(0.5f * (SlotColumn - BubbleSize) - 5.f, -5.f, 0.f, 0.f));
		Place(Canvas, Root, FVector2D(0.5f, 1.f), FVector2D(0.f, -12.f));
	}

	// ── Puntos (arriba a la derecha): concha y número en una etiqueta de arena ──
	{
		ScoreRoot = Make<UOverlay>(Tree);
		ScoreText = MakeText(Tree, TEXT("ScoreText"), FText::AsNumber(0), TEXT("Bold"), 30, NavyText, false);
		AddAt(ScoreRoot, MakeCard(Tree, TNHUDArt::SandTagTexture(), TagMargin, ScoreText, FMargin(66.f, 16.f, 30.f, 16.f)), HAlign_Left, VAlign_Center,
			FMargin(22.f, 0.f, 0.f, 0.f));
		UImage* ShellImg = MakeImage(Tree, TNHUDArt::ShellIcon(), FVector2D(82.f, 82.f));
		ShellImg->SetRenderTransformAngle(-12.f);
		AddAt(ScoreRoot, ShellImg, HAlign_Left, VAlign_Center);
		ScoreRoot->SetRenderTransformPivot(FVector2D(0.3f, 0.5f));
		Place(Canvas, ScoreRoot, FVector2D(1.f, 0.f), FVector2D(-28.f, 16.f));
	}

	// ── Pista de la playa al mar (arriba en el centro) ──
	{
		TrackRoot = Make<UOverlay>(Tree, TEXT("SeaTrack"));
		AddAt(TrackRoot, MakeImage(Tree, TNHUDArt::TrackTexture(), FVector2D(TrackW, TrackH)), HAlign_Fill, VAlign_Fill);
		// Arena que ya se ha tragado la tormenta (crece desde el nido).
		StormShade = Make<UImage>(Tree);
		StormShade->SetBrush(Rounded(TNHUDArt::Hex(0x0B1020, 0.6f), 18.f));
		AddAt(TrackRoot, MakeSize(Tree, StormShade, 1.f, TrackH - 24.f), HAlign_Left, VAlign_Center, FMargin(8.f, 0.f, 0.f, 0.f));
		AddAt(TrackRoot, MakeImage(Tree, TNHUDArt::NestIcon(), FVector2D(78.f, 72.f)), HAlign_Left, VAlign_Center, FMargin(-50.f, -10.f, 0.f, 0.f));
		AddAt(TrackRoot, MakeImage(Tree, TNHUDArt::SeaIcon(), FVector2D(92.f, 75.f)), HAlign_Right, VAlign_Center, FMargin(0.f, -12.f, -52.f, 0.f));
		for (const FLinearColor& Tint : MateColors)
		{
			UImage* Mate = MakeImage(Tree, TNHUDArt::ShellDotIcon(), FVector2D(30.f, 30.f));
			Mate->SetColorAndOpacity(Tint);
			Mate->SetVisibility(ESlateVisibility::Collapsed);
			AddAt(TrackRoot, Mate, HAlign_Left, VAlign_Center);
			MateMarkers.Add(Mate);
		}
		StormMarker = MakeImage(Tree, TNHUDArt::StormIcon(), FVector2D(66.f, 54.f));
		StormMarker->SetVisibility(ESlateVisibility::Collapsed);
		AddAt(TrackRoot, StormMarker, HAlign_Left, VAlign_Center);
		MiniFace = MakeImage(Tree, TNHUDFaces::TurtleFace(ETNTurtleFace::Happy), FVector2D(54.f, 54.f));
		AddAt(TrackRoot, MiniFace, HAlign_Left, VAlign_Center);
		TrackRoot->SetVisibility(ESlateVisibility::Collapsed);
		Place(Canvas, MakeSize(Tree, TrackRoot, TrackW, TrackH), FVector2D(0.5f, 0.f), FVector2D(0.f, 34.f));
	}

	// ── Avisos: carteles azul marino con ola ──
	{
		UHorizontalBox* StormRow = Make<UHorizontalBox>(Tree);
		if (UHorizontalBoxSlot* S = StormRow->AddChildToHorizontalBox(MakeImage(Tree, TNHUDArt::StormIcon(), FVector2D(58.f, 48.f)))) { S->SetVerticalAlignment(VAlign_Center); }
		StormText = MakeText(Tree, nullptr, FText::GetEmpty(), TEXT("Bold"), 23, TNHUDArt::Hex(0xFF9A85));
		if (UHorizontalBoxSlot* S = StormRow->AddChildToHorizontalBox(StormText)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f)); }
		StormBanner = MakeCard(Tree, TNHUDArt::CardTexture(), CardMargin, StormRow, FMargin(24.f, 14.f, 30.f, 36.f));
		StormBanner->SetVisibility(ESlateVisibility::Collapsed);
		StormBanner->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Place(Canvas, StormBanner, FVector2D(0.5f, 0.f), FVector2D(0.f, 118.f));

		DownText = MakeText(Tree, nullptr, FText::GetEmpty(), TEXT("Bold"), 21, TNHUDArt::SandC);
		DownBanner = MakeFaceCard(Tree, ETNTurtleFace::Down, 92.f, DownText);
		DownBanner->SetVisibility(ESlateVisibility::Collapsed);
		Place(Canvas, DownBanner, FVector2D(0.5f, 0.5f), FVector2D(0.f, 150.f));

		UVerticalBox* Col = Make<UVerticalBox>(Tree);
		if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(MakeText(Tree, nullptr, NSLOCTEXT("TNHUD", "Reviving", "Dando la vuelta a tu compañero..."), TEXT("Bold"), 16, FLinearColor::White)))
		{
			S->SetHorizontalAlignment(HAlign_Center);
		}
		ReviveBar = Make<UProgressBar>(Tree);
		ReviveBar->SetWidgetStyle(Bar(7.f));
		ReviveBar->SetFillColorAndOpacity(TNHUDArt::SeaLight);
		if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(MakeSize(Tree, ReviveBar, 240.f, 14.f))) { S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f)); }
		ReviveBanner = MakeCard(Tree, TNHUDArt::CardTexture(), CardMargin, Col, FMargin(24.f, 14.f, 24.f, 36.f));
		ReviveBanner->SetVisibility(ESlateVisibility::Collapsed);
		Place(Canvas, ReviveBanner, FVector2D(0.5f, 0.5f), FVector2D(0.f, 90.f));
	}
}

void UTN_RunHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Time += InDeltaTime;
	TickBadge(InDeltaTime);
	TickInventory(InDeltaTime);
	TickTrack(InDeltaTime);
	TickScore(InDeltaTime);
	TickAlerts(InDeltaTime);
}

void UTN_RunHUDWidget::TickBadge(float DeltaTime)
{
	// Energía del salvavidas (suavizada), zona bloqueada por el peso y latido al quedarse sin aliento.
	float Energy = StaminaBar && StaminaBar->IsVisible() ? StaminaBar->GetPercent() : 1.f;
	if (CVarHUDEnergy.GetValueOnGameThread() >= 0.f) { Energy = FMath::Clamp(CVarHUDEnergy.GetValueOnGameThread(), 0.f, 1.f); }
	ShownEnergy = FMath::FInterpTo(ShownEnergy, Energy, DeltaTime, 7.f);
	const bool bTired = ExhaustedRoot && ExhaustedRoot->IsVisible();
	if (BadgeMID)
	{
		BadgeMID->SetScalarParameterValue(TEXT("Energy"), ShownEnergy);
		BadgeMID->SetScalarParameterValue(TEXT("Weight"), WeightPenaltyBar && WeightPenaltyBar->IsVisible() ? WeightPenaltyBar->GetPercent() : 0.f);
		BadgeMID->SetScalarParameterValue(TEXT("Exhausted"), bTired ? 1.f : 0.f);
	}
	if (bTired) { ExhaustedRoot->SetRenderScale(FVector2D(1.f + 0.07f * FMath::Abs(FMath::Sin(Time * 7.f)))); }

	const APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const APlayerState* PS = PC ? PC->PlayerState.Get() : nullptr;
	if (NameText)
	{
		const FString Shown = PS && !PS->GetPlayerName().IsEmpty() ? PS->GetPlayerName() : FString(TEXT("Tortuga"));
		if (!NameText->GetText().ToString().Equals(Shown)) { NameText->SetText(FText::FromString(Shown)); }
	}

	// Cara según cómo va la tortuga.
	const ETNTurtleFace Prev = static_cast<ETNTurtleFace>(ShownFace);
	ETNTurtleFace Face = FaceFor(PS, Pawn, ShownEnergy, bTired, Prev);
	if (CVarHUDFace.GetValueOnGameThread() >= 0) { Face = static_cast<ETNTurtleFace>(FMath::Clamp(CVarHUDFace.GetValueOnGameThread(), 0, static_cast<int32>(ETNTurtleFace::Win))); }
	if (Face != Prev)
	{
		ShownFace = static_cast<uint8>(Face);
		SetImageTexture(FaceImage, TNHUDFaces::TurtleFace(Face));
		SetImageTexture(MiniFace, TNHUDFaces::TurtleFace(Face));
		FacePop = 1.f;
	}
	FacePop = FMath::Max(0.f, FacePop - DeltaTime * 4.f);

	// Hablando por la voz de proximidad: la cara rebota como si hablara y sale el bocadillo con las barras.
	const UProximityVoiceComponent* Voice = Pawn ? Pawn->FindComponentByClass<UProximityVoiceComponent>() : nullptr;
	const int32 ForceTalk = CVarHUDTalk.GetValueOnGameThread();
	const bool bTalking = ForceTalk >= 0 ? ForceTalk > 0 : (Voice && Voice->IsHeardSpeaking());
	float Scale = 1.f + 0.22f * FMath::Sin(FacePop * PI);
	if (bTalking) { Scale *= 1.f + 0.07f * FMath::Abs(FMath::Sin(Time * 17.f)); }
	if (Face == ETNTurtleFace::Panting) { Scale *= 1.f + 0.035f * FMath::Sin(Time * 9.f); }
	if (FaceImage) { FaceImage->SetRenderScale(FVector2D(Scale, Scale)); }
	if (TalkBubble)
	{
		TalkBubble->SetVisibility(bTalking ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bTalking)
		{
			TalkBubble->SetRenderTransformAngle(5.f * FMath::Sin(Time * 6.f));
			AnimateTalkBars(TalkBars, 0, Time, 0.f);
		}
	}
}

void UTN_RunHUDWidget::TickInventory(float DeltaTime)
{
	if (ItemImages.Num() < 2 || SlotTags.Num() < 2) { return; }
	// La clase base pinta el equipado y el guardado en sus imágenes (ocultas); aquí se reparten entre las dos
	// burbujas. Si solo se han intercambiado (rotar objetos), los objetos se quedan donde estaban y es el aro de
	// cuerda el que rueda a la otra burbuja.
	auto IconOf = [](const UImage* I) -> UObject*
	{
		return I && I->GetColorAndOpacity().A > 0.01f ? I->GetBrush().GetResourceObject() : nullptr;
	};
	UObject* Equipped = IconOf(SlotEquippedImage);
	UObject* Stored = IconOf(SlotStoredImage);
	if (Equipped != LastEquippedIcon.Get() || Stored != LastStoredIcon.Get())
	{
		const bool bSwapped = (Equipped || Stored) && Equipped == LastStoredIcon.Get() && Stored == LastEquippedIcon.Get();
		if (bSwapped) { EquippedSide = 1 - EquippedSide; }
		LastEquippedIcon = Equipped;
		LastStoredIcon = Stored;
		UObject* Shown[2];
		Shown[EquippedSide] = Equipped;
		Shown[1 - EquippedSide] = Stored;
		for (int32 i = 0; i < 2; ++i)
		{
			SetImageTexture(ItemImages[i], Cast<UTexture2D>(Shown[i]));
			ItemImages[i]->SetColorAndOpacity(Shown[i] ? FLinearColor::White : FLinearColor::Transparent);
		}
		SlotTags[EquippedSide]->SetText(NSLOCTEXT("TNHUD", "InHand", "EN LA ALETA"));
		SlotTags[1 - EquippedSide]->SetText(NSLOCTEXT("TNHUD", "Stored", "EN EL CAPARAZÓN"));
	}
	// El aro rueda (se traslada y gira) hasta la burbuja de la aleta.
	const float Pitch = SlotColumn + SlotGap;
	RopeX = FMath::FInterpTo(RopeX, EquippedSide * Pitch, DeltaTime, 12.f);
	if (RopeImage)
	{
		RopeImage->SetRenderTranslation(FVector2D(RopeX, 0.f));
		RopeImage->SetRenderTransformAngle(RopeX / Pitch * 180.f);
	}
}

void UTN_RunHUDWidget::TickTrack(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !TrackRoot) { return; }
	LookupTimer -= DeltaTime;
	if ((!Generator.IsValid() || !Storm.IsValid()) && LookupTimer <= 0.f)
	{
		LookupTimer = 1.f;
		if (!Generator.IsValid()) { for (TActorIterator<ATN_ProcMapGenerator> It(World); It; ++It) { Generator = *It; break; } }
		if (!Storm.IsValid()) { for (TActorIterator<ATN_PathStorm> It(World); It; ++It) { Storm = *It; break; } }
	}
	const ATN_ProcMapGenerator* Gen = Generator.Get();
	const float Length = Gen && Gen->IsMapReady() ? Gen->GetMainPathLength() : 0.f;
	TrackRoot->SetVisibility(Length > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Length <= 0.f) { return; }

	const float Usable = (TrackTo - TrackFrom) * TrackW;
	auto ToX = [&](float Progress) { return TrackFrom * TrackW + Usable * FMath::Clamp(Progress / Length, 0.f, 1.f); };

	// Tu cara avanza del nido al mar.
	const APlayerController* PC = GetOwningPlayer();
	const APawn* Own = PC ? PC->GetPawn() : nullptr;
	if (Own) { ShownProgress = FMath::FInterpTo(ShownProgress, Gen->GetPathProgress(Own->GetActorLocation()), DeltaTime, 4.f); }
	if (MiniFace) { MiniFace->SetRenderTranslation(FVector2D(ToX(ShownProgress) - 27.f, -14.f + 2.f * FMath::Sin(Time * 5.f))); }

	// Compañeros: caparazones de su color (el mismo orden y color que en la tripulación de la izquierda).
	const TArray<const APlayerState*> Crew = CrewOf(World, PC ? PC->PlayerState.Get() : nullptr);
	for (int32 m = 0; m < MateMarkers.Num(); ++m)
	{
		const APawn* P = Crew.IsValidIndex(m) ? Crew[m]->GetPawn() : nullptr;
		MateMarkers[m]->SetVisibility(P ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (P) { MateMarkers[m]->SetRenderTranslation(FVector2D(ToX(Gen->GetPathProgress(P->GetActorLocation())) - 15.f, 17.f)); }
	}

	// La tormenta: su nube detrás de todos y la arena que ya se ha tragado.
	const ATN_PathStorm* S = Storm.Get();
	const bool bStorm = S && S->IsStormActive();
	StormMarker->SetVisibility(bStorm ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	StormShade->SetVisibility(bStorm ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bStorm)
	{
		ShownStorm = FMath::FInterpTo(ShownStorm, FMath::Max(0.f, S->GetFrontProgress()), DeltaTime, 3.f);
		const float X = ToX(ShownStorm);
		StormMarker->SetRenderTranslation(FVector2D(X - 44.f, -14.f + 2.5f * FMath::Sin(Time * 3.f)));
		if (USizeBox* Box = Cast<USizeBox>(StormShade->GetParent())) { Box->SetWidthOverride(FMath::Max(1.f, X - 8.f)); }
	}
}

void UTN_RunHUDWidget::TickScore(float DeltaTime)
{
	if (!ScoreText || !ScoreRoot) { return; }
	const FString Now = ScoreText->GetText().ToString();
	if (!LastScore.IsEmpty() && Now != LastScore) { ScorePop = 1.f; }
	LastScore = Now;
	ScorePop = FMath::Max(0.f, ScorePop - DeltaTime * 2.5f);
	const float Scale = 1.f + 0.3f * FMath::Sin(ScorePop * PI);
	ScoreRoot->SetRenderScale(FVector2D(Scale, Scale));
	ScoreRoot->SetRenderTransformAngle(-8.f * FMath::Sin(ScorePop * PI * 2.f));
}

void UTN_RunHUDWidget::TickAlerts(float DeltaTime)
{
	const APlayerController* PC = GetOwningPlayer();
	const ATN_CoopPlayerState* PS = PC ? PC->GetPlayerState<ATN_CoopPlayerState>() : nullptr;

	// Tormenta: cuenta atrás mientras se está dentro.
	const bool bInStorm = PS && PS->DeathZoneTimeRemaining >= 0.f && PS->bIsAlive;
	if (StormBanner)
	{
		StormBanner->SetVisibility(bInStorm ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bInStorm && StormText)
		{
			FNumberFormattingOptions OneDecimal;
			OneDecimal.SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(1);
			StormText->SetText(FText::Format(NSLOCTEXT("TNHUD", "Storm", "¡La tormenta te alcanza! ¡Al agua!   {0}"), FText::AsNumber(PS->DeathZoneTimeRemaining, &OneDecimal)));
			StormBanner->SetRenderScale(FVector2D(1.f + 0.04f * FMath::Abs(FMath::Sin(Time * 6.f))));
		}
	}

	// Panza arriba: lo que queda para que un compañero te dé la vuelta.
	const bool bDown = PS && PS->bIsDBNO;
	if (DownBanner)
	{
		DownBanner->SetVisibility(bDown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bDown && DownText)
		{
			DownText->SetText(FText::Format(NSLOCTEXT("TNHUD", "Down", "¡Panza arriba!\nUn compañero puede darte la vuelta:  {0} s"),
				FText::AsNumber(FMath::Max(0, FMath::CeilToInt(PS->DBNOBleedoutTimeRemaining)))));
		}
	}

	// Dando la vuelta a un compañero.
	const ATortugaCharacter* Turtle = PC ? Cast<ATortugaCharacter>(PC->GetPawn()) : nullptr;
	const bool bReviving = Turtle && Turtle->bIsReviving;
	if (ReviveBanner)
	{
		ReviveBanner->SetVisibility(bReviving ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bReviving && ReviveBar) { ReviveBar->SetPercent(FMath::Clamp(Turtle->ReviveProgress, 0.f, 1.f)); }
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Cartel de estado, resultados, tripulación y mensajes
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

	// ── Cartel de estado (arriba a la izquierda, fuera de la carrera): azul marino con ola, ancla y dos líneas ──
	{
		UHorizontalBox* Row = Make<UHorizontalBox>(Tree);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(MakeImage(Tree, TNHUDArt::AnchorIcon(), FVector2D(40.f, 40.f)))) { S->SetVerticalAlignment(VAlign_Center); }
		RootContainer = Make<UVerticalBox>(Tree, TEXT("RootContainer"));
		PrimaryText = MakeText(Tree, TEXT("PrimaryText"), FText::GetEmpty(), TEXT("Bold"), 21, TNHUDArt::SandC);
		PrimaryText->SetAutoWrapText(true);
		RootContainer->AddChildToVerticalBox(PrimaryText);
		SecondaryText = MakeText(Tree, TEXT("SecondaryText"), FText::GetEmpty(), TEXT("Regular"), 15, TNHUDArt::Foam);
		SecondaryText->SetAutoWrapText(true);
		if (UVerticalBoxSlot* S = RootContainer->AddChildToVerticalBox(SecondaryText)) { S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f)); }
		USizeBox* Limit = MakeSize(Tree, RootContainer, 0.f, 0.f);
		Limit->SetMaxDesiredWidth(470.f);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Limit)) { S->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f)); S->SetVerticalAlignment(VAlign_Center); }
		UBorder* Status = MakeCard(Tree, TNHUDArt::CardTexture(), CardMargin, Row, FMargin(20.f, 14.f, 26.f, 36.f));
		Status->SetHorizontalAlignment(HAlign_Left);
		Status->SetRenderTransformAngle(-1.5f);
		StatusCard = Status;
		Place(Canvas, Status, FVector2D(0.f, 0.f), FVector2D(22.f, 14.f));
	}

	// ── Fuera de carrera (te han eliminado): cara mareada ──
	{
		UVerticalBox* Lines = Make<UVerticalBox>(Tree);
		Lines->AddChildToVerticalBox(MakeText(Tree, nullptr, NSLOCTEXT("TNHUD", "OutTitle", "¡Fuera de carrera!"), TEXT("Bold"), 24, TNHUDArt::Hex(0xFF9A85)));
		if (UVerticalBoxSlot* S = Lines->AddChildToVerticalBox(MakeText(Tree, nullptr, NSLOCTEXT("TNHUD", "OutHint", "Anima a los demás hasta que lleguen al agua."), TEXT("Regular"), 15, TNHUDArt::Foam)))
		{
			S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
		}
		UBorder* Out = MakeFaceCard(Tree, ETNTurtleFace::Down, 88.f, Lines);
		Out->SetVisibility(ESlateVisibility::Collapsed);
		OutCard = Out;
		Place(Canvas, Out, FVector2D(0.5f, 0.f), FVector2D(0.f, 120.f));
	}

	// ── Tripulación (izquierda): la cara de cada compañero en un aro de su color, su nombre y sus bocadillos ──
	{
		UVerticalBox* Crew = Make<UVerticalBox>(Tree);
		for (int32 i = 0; i < 3; ++i)
		{
			UOverlay* Row = Make<UOverlay>(Tree);
			UOverlay* Portrait = Make<UOverlay>(Tree);
			UImage* RingImg = Make<UImage>(Tree);
			RingImg->SetBrush(Rounded(MateColors[i], 36.f, TNHUDArt::Navy, 3.f));
			CrewRings.Add(RingImg);
			AddAt(Portrait, MakeSize(Tree, RingImg, 72.f, 72.f), HAlign_Center, VAlign_Center);
			UImage* FaceImg = MakeImage(Tree, TNHUDFaces::TurtleFace(ETNTurtleFace::Happy), FVector2D(68.f, 68.f));
			CrewFaces.Add(FaceImg);
			AddAt(Portrait, FaceImg, HAlign_Center, VAlign_Center);
			UHorizontalBox* Line = Make<UHorizontalBox>(Tree);
			Line->AddChildToHorizontalBox(MakeSize(Tree, Portrait, 76.f, 76.f));
			UTextBlock* NameTxt = MakeText(Tree, nullptr, FText::GetEmpty(), TEXT("Bold"), 13, TNHUDArt::Cream);
			CrewNames.Add(NameTxt);
			UBorder* NamePill = Make<UBorder>(Tree);
			StylePanel(NamePill, TNHUDArt::Hex(0x0A1C38, 0.75f), 10.f, FMargin(10.f, 3.f, 12.f, 4.f), FLinearColor::Transparent, 0.f);
			NamePill->SetContent(NameTxt);
			if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(NamePill)) { S->SetVerticalAlignment(VAlign_Bottom); S->SetPadding(FMargin(-10.f, 0.f, 0.f, 4.f)); }
			AddAt(Row, Line, HAlign_Left, VAlign_Bottom);
			// Voz: bocadillo pequeño con barras arriba a la derecha de la cara.
			UOverlay* Talk = MakeTalkBubble(Tree, 46.f, CrewTalkBars);
			Talk->SetVisibility(ESlateVisibility::Collapsed);
			CrewTalk.Add(Talk);
			AddAt(Row, Talk, HAlign_Left, VAlign_Top, FMargin(56.f, -6.f, 0.f, 0.f));
			// Frase del chat rápido: bocadillo a la derecha, pasado el de la voz para no pisarse, con la cola hacia la cara;
			// crece hacia arriba (anclado por abajo, por encima del nombre) si la frase ocupa varias líneas.
			UTextBlock* Say = nullptr;
			UBorder* Bubble = MakeChatBubble(Tree, 15, 230.f, Say);
			Bubbles.Add(Bubble);
			BubbleTexts.Add(Say);
			AddAt(Row, Bubble, HAlign_Left, VAlign_Bottom, FMargin(108.f, 0.f, 0.f, 36.f));
			Row->SetVisibility(ESlateVisibility::Collapsed);
			CrewRows.Add(Row);
			if (UVerticalBoxSlot* S = Crew->AddChildToVerticalBox(Row)) { S->SetPadding(FMargin(0.f, 34.f, 0.f, 0.f)); }
		}
		CrewPlayerIds.Init(INDEX_NONE, 3);
		CrewFaceShown.Init(0xFF, 3);
		Place(Canvas, Crew, FVector2D(0.f, 0.3f), FVector2D(20.f, 0.f));
	}

	// ── Tu frase: bocadillo junto a tu distintivo (abajo a la izquierda) ──
	{
		UTextBlock* Say = nullptr;
		UBorder* Bubble = MakeChatBubble(Tree, 16, 250.f, Say);
		Bubbles.Add(Bubble);
		BubbleTexts.Add(Say);
		Place(Canvas, Bubble, FVector2D(0.f, 1.f), FVector2D(226.f, -150.f));
	}
	BubbleTime.Init(0.f, Bubbles.Num());

	// El chat global de la clase base no se usa (las frases salen en bocadillos); la caja existe oculta.
	{
		ChatHistoryBox = Make<UVerticalBox>(Tree, TEXT("ChatHistoryBox"));
		ChatHistoryBox->SetVisibility(ESlateVisibility::Collapsed);
		Place(Canvas, ChatHistoryBox, FVector2D(0.f, 0.f), FVector2D(0.f, 0.f));
	}

	// ── Aviso de espectador (abajo, sobre el inventario) ──
	{
		SpectatorHint = MakeText(Tree, TEXT("SpectatorHint"), FText::GetEmpty(), TEXT("Bold"), 15, TNHUDArt::SandC);
		SpectatorHint->SetVisibility(ESlateVisibility::Collapsed);
		Place(Canvas, SpectatorHint, FVector2D(0.5f, 1.f), FVector2D(0.f, -170.f));
	}

	// ── Resultados: el mar oscurecido y un cartel con la cara, el puesto y la clasificación en conchas ──
	{
		UVerticalBox* Board = Make<UVerticalBox>(Tree);
		ResultsFace = MakeImage(Tree, TNHUDFaces::TurtleFace(ETNTurtleFace::Win), FVector2D(150.f, 150.f));
		if (UVerticalBoxSlot* S = Board->AddChildToVerticalBox(ResultsFace)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, -70.f, 0.f, 0.f)); }
		ResultsTitle = MakeText(Tree, TEXT("ResultsTitle"), FText::GetEmpty(), TEXT("Bold"), 44, TNHUDArt::SandC);
		ResultsTitle->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = Board->AddChildToVerticalBox(ResultsTitle)) { S->SetHorizontalAlignment(HAlign_Center); }
		ResultsRankText = MakeText(Tree, TEXT("ResultsRankText"), FText::GetEmpty(), TEXT("Bold"), 24, FLinearColor::White);
		if (UVerticalBoxSlot* S = Board->AddChildToVerticalBox(ResultsRankText)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f)); }
		ResultsTimeText = MakeText(Tree, TEXT("ResultsTimeText"), FText::GetEmpty(), TEXT("Regular"), 20, TNHUDArt::Foam);
		if (UVerticalBoxSlot* S = Board->AddChildToVerticalBox(ResultsTimeText)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 2.f, 0.f, 14.f)); }

		// Clasificación: cuatro filas (puesto, nombre, tiempo, puntos con su concha), alternando el fondo.
		TObjectPtr<UTextBlock>* Cells[4][4] = {
			{ &Row1RankText, &Row1NameText, &Row1TimeText, &Row1ScoreText },
			{ &Row2RankText, &Row2NameText, &Row2TimeText, &Row2ScoreText },
			{ &Row3RankText, &Row3NameText, &Row3TimeText, &Row3ScoreText },
			{ &Row4RankText, &Row4NameText, &Row4TimeText, &Row4ScoreText } };
		const float Widths[4] = { 60.f, 270.f, 130.f, 90.f };
		for (int32 r = 0; r < 4; ++r)
		{
			UHorizontalBox* Line = Make<UHorizontalBox>(Tree);
			for (int32 c = 0; c < 4; ++c)
			{
				UTextBlock* Cell = MakeText(Tree, nullptr, FText::GetEmpty(), c == 1 ? TEXT("Bold") : TEXT("Regular"), 19, c == 0 ? TNHUDArt::SandC : FLinearColor::White);
				Cell->SetJustification(c >= 2 ? ETextJustify::Right : ETextJustify::Left);
				*Cells[r][c] = Cell;
				if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(MakeSize(Tree, Cell, Widths[c], 0.f))) { S->SetVerticalAlignment(VAlign_Center); }
			}
			if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(MakeImage(Tree, TNHUDArt::ShellIcon(), FVector2D(30.f, 30.f))))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
			}
			UBorder* Stripe = Make<UBorder>(Tree);
			StylePanel(Stripe, (r % 2) == 0 ? TNHUDArt::Hex(0x62D2EA, 0.1f) : FLinearColor::Transparent, 12.f, FMargin(14.f, 5.f), FLinearColor::Transparent, 0.f);
			Stripe->SetContent(Line);
			if (UVerticalBoxSlot* S = Board->AddChildToVerticalBox(Stripe)) { S->SetPadding(FMargin(0.f, 1.f)); }
		}
		ResultsCountdown = MakeText(Tree, TEXT("ResultsCountdown"), FText::GetEmpty(), TEXT("Regular"), 17, TNHUDArt::Foam);
		if (UVerticalBoxSlot* S = Board->AddChildToVerticalBox(ResultsCountdown)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f)); }

		UBorder* BoardCard = MakeCard(Tree, TNHUDArt::CardTexture(), CardMargin, Board, FMargin(42.f, 30.f, 42.f, 50.f));
		UBorder* Dim = Make<UBorder>(Tree);
		StylePanel(Dim, TNHUDArt::Hex(0x06121F, 0.55f), 0.f, FMargin(0.f), FLinearColor::Transparent, 0.f);
		Dim->SetContent(BoardCard);
		Dim->SetHorizontalAlignment(HAlign_Center);
		Dim->SetVerticalAlignment(VAlign_Center);
		ResultsOverlay = Dim;
		ResultsOverlay->SetVisibility(ESlateVisibility::Collapsed);
		UCanvasPanelSlot* DimSlot = Canvas->AddChildToCanvas(Dim);
		DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DimSlot->SetOffsets(FMargin(0.f));
	}
}

void UTN_RunFlowHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Time += InDeltaTime;
	const UWorld* World = GetWorld();
	const ATN_CoopGameState* GS = World ? World->GetGameState<ATN_CoopGameState>() : nullptr;
	const APlayerController* PC = GetOwningPlayer();
	const ATN_CoopPlayerState* PS = PC ? PC->GetPlayerState<ATN_CoopPlayerState>() : nullptr;
	const bool bRacing = GS && GS->MatchFlowState == ETNMatchFlowState::InProgress;

	// En carrera el objetivo ya lo cuenta la pista del mar: sin cartel de estado.
	if (StatusCard) { StatusCard->SetVisibility(bRacing ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }

	const bool bOut = bRacing && PS && PS->bIsEliminated;
	if (OutCard)
	{
		OutCard->SetVisibility(bOut ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bOut) { OutCard->SetRenderTransformAngle(2.f * FMath::Sin(Time * 2.f)); }
	}

	// Resultados: ojos de estrella si llegaste al agua, mareada si no; el título, del color de la medalla.
	if (ResultsOverlay && ResultsOverlay->IsVisible() && PS)
	{
		const bool bArrived = PS->bHasFinishedRun && !PS->bIsEliminated && PS->FinishRank > 0;
		SetImageTexture(ResultsFace, TNHUDFaces::TurtleFace(bArrived ? ETNTurtleFace::Win : ETNTurtleFace::Down));
		if (ResultsFace) { ResultsFace->SetRenderScale(FVector2D(1.f + 0.05f * FMath::Sin(Time * 3.f))); }
		if (ResultsTitle)
		{
			FLinearColor Medal = TNHUDArt::Hex(0xFF9A85);
			if (bArrived)
			{
				Medal = PS->FinishRank == 1 ? TNHUDArt::Gold : PS->FinishRank == 2 ? TNHUDArt::Hex(0xDDE6EE) : PS->FinishRank == 3 ? TNHUDArt::Hex(0xE8A56B) : TNHUDArt::SandC;
			}
			ResultsTitle->SetColorAndOpacity(FSlateColor(Medal));
		}
	}

	TickCrew(InDeltaTime);
}

void UTN_RunFlowHUDWidget::TickCrew(float DeltaTime)
{
	const APlayerController* PC = GetOwningPlayer();
	TArray<const APlayerState*> Crew = CrewOf(GetWorld(), PC ? PC->PlayerState.Get() : nullptr);
	const int32 Preview = FMath::Min(CVarHUDCrew.GetValueOnGameThread(), CrewRows.Num());
	if (Preview > 0 && PC && PC->PlayerState)
	{
		Crew.Init(PC->PlayerState.Get(), Preview);
		if (Bubbles.IsValidIndex(0) && BubbleTime[0] <= 0.f) { ShowBubble(0, NSLOCTEXT("TNHUD", "PreviewSay", "¡Por aquí, que hay conchas!")); }
	}
	for (int32 i = 0; i < CrewRows.Num(); ++i)
	{
		const APlayerState* PS = Crew.IsValidIndex(i) ? Crew[i] : nullptr;
		CrewRows[i]->SetVisibility(PS ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		CrewPlayerIds[i] = PS ? PS->GetPlayerId() : INDEX_NONE;
		if (!PS) { continue; }
		const APawn* Pawn = PS->GetPawn();
		float Energy = 1.f;
		bool bExhausted = false;
		EnergyOf(Pawn, Energy, bExhausted);
		const ETNTurtleFace Face = FaceFor(PS, Pawn, Energy, bExhausted, static_cast<ETNTurtleFace>(CrewFaceShown[i]));
		if (static_cast<uint8>(Face) != CrewFaceShown[i])
		{
			CrewFaceShown[i] = static_cast<uint8>(Face);
			SetImageTexture(CrewFaces[i], TNHUDFaces::TurtleFace(Face));
		}
		const FString PlayerName = PS->GetPlayerName();
		if (!CrewNames[i]->GetText().ToString().Equals(PlayerName)) { CrewNames[i]->SetText(FText::FromString(PlayerName)); }
		// Voz: el bocadillo con barras mientras llega su audio.
		const UProximityVoiceComponent* Voice = Pawn ? Pawn->FindComponentByClass<UProximityVoiceComponent>() : nullptr;
		const bool bTalking = (Voice && Voice->IsHeardSpeaking()) || (Preview > 1 && i == 1);
		CrewTalk[i]->SetVisibility(bTalking ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bTalking) { AnimateTalkBars(CrewTalkBars, i * 4, Time, i * 2.1f); }
		CrewFaces[i]->SetRenderScale(FVector2D(bTalking ? 1.f + 0.06f * FMath::Abs(FMath::Sin(Time * 17.f + i)) : 1.f));
	}

	// Bocadillos del chat: entran con un rebote y se van encogiendo al final.
	for (int32 b = 0; b < Bubbles.Num(); ++b)
	{
		if (BubbleTime[b] <= 0.f) { continue; }
		BubbleTime[b] -= DeltaTime;
		const float In = FMath::Clamp((BubbleLife - BubbleTime[b]) / 0.18f, 0.f, 1.f);
		const float Out = FMath::Clamp(BubbleTime[b] / 0.3f, 0.f, 1.f);
		Bubbles[b]->SetRenderScale(FVector2D(0.55f + 0.45f * FMath::Min(In, Out) + 0.08f * FMath::Sin(In * PI)));
		Bubbles[b]->SetRenderOpacity(Out);
		if (BubbleTime[b] <= 0.f) { Bubbles[b]->SetVisibility(ESlateVisibility::Collapsed); }
	}
}

void UTN_RunFlowHUDWidget::ShowBubble(int32 Row, const FText& MessageText)
{
	if (!Bubbles.IsValidIndex(Row) || !BubbleTexts.IsValidIndex(Row)) { return; }
	BubbleTexts[Row]->SetText(MessageText);
	Bubbles[Row]->SetVisibility(ESlateVisibility::HitTestInvisible);
	Bubbles[Row]->SetRenderOpacity(1.f);
	BubbleTime[Row] = BubbleLife;
}

void UTN_RunFlowHUDWidget::OnQuickChatEntryReceived_Implementation(int32 Sequence, const FText& SenderName, const FText& MessageText,
	UTexture2D* Icon, float ServerTimeSeconds)
{
	// Sin chat global: la frase sale en un bocadillo junto a la cara de quien la dice. Las viejas (el historial que
	// se repasa al crear el widget) no se enseñan.
	const UWorld* World = GetWorld();
	const ATN_CoopGameState* GS = World ? World->GetGameState<ATN_CoopGameState>() : nullptr;
	if (!GS || GS->GetServerWorldTimeSeconds() - ServerTimeSeconds > 6.0) { return; }
	int32 SenderId = INDEX_NONE;
	for (const FTN_QuickChatEntry& Entry : GS->QuickChatHistory)
	{
		if (Entry.Sequence == Sequence) { SenderId = Entry.SenderPlayerId; }
	}
	const APlayerController* PC = GetOwningPlayer();
	const APlayerState* Own = PC ? PC->PlayerState.Get() : nullptr;
	int32 Row = Bubbles.Num() - 1;
	if (SenderId != INDEX_NONE && !(Own && Own->GetPlayerId() == SenderId))
	{
		TickCrew(0.f);
		Row = CrewPlayerIds.IndexOfByKey(SenderId);
		if (Row == INDEX_NONE) { return; }
	}
	ShowBubble(Row, MessageText);
}

// ─────────────────────────────────────────────────────────────────────────────
// Rueda radial (emotes y frases)
// ─────────────────────────────────────────────────────────────────────────────

void UTN_RunRadialWheelWidget::NativeOnInitialized()
{
	BuildTree();
	Super::NativeOnInitialized();
}

void UTN_RunRadialWheelWidget::SetTitle(const FText& InTitle)
{
	PendingTitle = InTitle;
	if (TitleText) { TitleText->SetText(InTitle); }
}

void UTN_RunRadialWheelWidget::BuildTree()
{
	if (!WidgetTree || Canvas) { return; }
	UWidgetTree* Tree = WidgetTree;
	Canvas = Make<UCanvasPanel>(Tree, TEXT("WheelCanvas"));
	Tree->RootWidget = Canvas;

	// Velo azul marino sobre la partida.
	UBorder* Veil = Make<UBorder>(Tree);
	StylePanel(Veil, TNHUDArt::Hex(0x06121F, 0.35f), 0.f, FMargin(0.f), FLinearColor::Transparent, 0.f);
	UCanvasPanelSlot* VeilSlot = Canvas->AddChildToCanvas(Veil);
	VeilSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	VeilSlot->SetOffsets(FMargin(0.f));

	// La rueda, centrada en la pantalla (desde donde se mide el ratón).
	UOverlay* Wheel = Make<UOverlay>(Tree);
	Disc = Make<UImage>(Tree);
	DiscMID = MakeUIMID(this, TEXT("/Game/UI/HUD/M_UI_RadialWheel.M_UI_RadialWheel"));
	if (DiscMID) { Disc->SetBrushFromMaterial(DiscMID); }
	AddAt(Wheel, Disc, HAlign_Fill, VAlign_Fill);
	SlotLayer = Make<UCanvasPanel>(Tree);
	AddAt(Wheel, SlotLayer, HAlign_Fill, VAlign_Fill);
	AddAt(Wheel, MakeImage(Tree, TNHUDFaces::TurtleFace(ETNTurtleFace::Happy), FVector2D(WheelSize * 0.3f, WheelSize * 0.3f)), HAlign_Center, VAlign_Center);
	UCanvasPanelSlot* WheelSlot = Canvas->AddChildToCanvas(MakeSize(Tree, Wheel, WheelSize, WheelSize));
	WheelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	WheelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	WheelSlot->SetAutoSize(true);
	WheelSlot->SetPosition(FVector2D::ZeroVector);

	// Título en una cinta arriba y la opción apuntada en una etiqueta de arena abajo.
	TitleText = MakeText(Tree, nullptr, PendingTitle, TEXT("Bold"), 22, FLinearColor::White);
	Place(Canvas, MakeCard(Tree, TNHUDArt::RibbonTexture(), RibbonMargin, TitleText, FMargin(48.f, 18.f, 48.f, 20.f)), FVector2D(0.5f, 0.5f),
		FVector2D(0.f, -WheelSize * 0.5f - 14.f));
	ChoiceText = MakeText(Tree, nullptr, FText::GetEmpty(), TEXT("Bold"), 20, NavyText, false);
	UBorder* Tag = MakeCard(Tree, TNHUDArt::SandTagTexture(), TagMargin, ChoiceText, FMargin(28.f, 15.f, 28.f, 16.f));
	ChoiceTag = Tag;
	Place(Canvas, Tag, FVector2D(0.5f, 0.5f), FVector2D(0.f, WheelSize * 0.5f + 22.f));
}

void UTN_RunRadialWheelWidget::BP_OnEntriesSet_Implementation(const TArray<FTN_RadialWheelEntryView>& InEntries)
{
	BuildTree();
	if (!SlotLayer) { return; }
	SlotLayer->ClearChildren();
	SlotWidgets.Reset();
	SlotLabels.Reset();
	const int32 N = InEntries.Num();
	if (DiscMID)
	{
		DiscMID->SetScalarParameterValue(TEXT("Slices"), static_cast<float>(FMath::Max(1, N)));
		DiscMID->SetScalarParameterValue(TEXT("Selected"), -1.f);
		DiscMID->SetScalarParameterValue(TEXT("OffsetDeg"), SelectionAngleOffsetDegrees);
	}
	if (N == 0) { return; }
	UWidgetTree* Tree = WidgetTree;
	const float Step = 2.f * PI / N;
	const float Offset = FMath::DegreesToRadians(SelectionAngleOffsetDegrees);
	for (int32 i = 0; i < N; ++i)
	{
		const FTN_RadialWheelEntryView& Entry = InEntries[i];
		// Mismo convenio que UTN_RadialWheelWidgetBase::UpdateInputVector: ángulo matemático con Y hacia arriba.
		const float Angle = Offset + Step * i;
		UVerticalBox* Box = Make<UVerticalBox>(Tree);
		if (Entry.Icon)
		{
			if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(MakeImage(Tree, Entry.Icon, FVector2D(44.f, 44.f)))) { S->SetHorizontalAlignment(HAlign_Center); }
		}
		UTextBlock* Label = MakeText(Tree, nullptr, Entry.Label, TEXT("Bold"), 15, TNHUDArt::Cream);
		Label->SetJustification(ETextJustify::Center);
		Label->SetAutoWrapText(true);
		USizeBox* Limit = MakeSize(Tree, Label, 0.f, 0.f);
		Limit->SetMaxDesiredWidth(118.f);
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Limit)) { S->SetHorizontalAlignment(HAlign_Center); }
		Box->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		if (!Entry.bEnabled) { Box->SetRenderOpacity(0.4f); }
		UCanvasPanelSlot* S = SlotLayer->AddChildToCanvas(Box);
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetAutoSize(true);
		S->SetPosition(FVector2D(FMath::Cos(Angle), -FMath::Sin(Angle)) * WheelSlotRadius);
		SlotWidgets.Add(Box);
		SlotLabels.Add(Label);
	}
	ShownSelection = -2;
}

void UTN_RunRadialWheelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Time += InDeltaTime;
	const int32 Sel = GetSelectedIndex();
	if (Sel != ShownSelection)
	{
		ShownSelection = Sel;
		if (DiscMID) { DiscMID->SetScalarParameterValue(TEXT("Selected"), static_cast<float>(Sel)); }
		const TArray<FTN_RadialWheelEntryView>& List = GetEntries();
		if (ChoiceText)
		{
			ChoiceText->SetText(List.IsValidIndex(Sel) ? List[Sel].Label : NSLOCTEXT("TNHUD", "WheelHint", "Apunta con el ratón"));
		}
		for (int32 i = 0; i < SlotLabels.Num(); ++i)
		{
			SlotLabels[i]->SetColorAndOpacity(FSlateColor(i == Sel ? TNHUDArt::Gold : TNHUDArt::Cream));
		}
	}
	for (int32 i = 0; i < SlotWidgets.Num(); ++i)
	{
		const float Scale = i == Sel ? 1.18f + 0.04f * FMath::Sin(Time * 8.f) : 1.f;
		SlotWidgets[i]->SetRenderScale(FVector2D(Scale, Scale));
	}
	if (ChoiceTag) { ChoiceTag->SetRenderTransformAngle(Sel >= 0 ? 2.f * FMath::Sin(Time * 5.f) : 0.f); }
}

int32 UTN_RunRadialWheelWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// La rueda la pinta el material: sin las líneas de la clase base.
	return UUserWidget::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}
