#include "UI/Briefing/TN_BriefingWidget.h"
#include "../HUD/TN_HUDFaces.h"
#include "../HUD/TN_HUDStyle.h"
#include "../Shop/TN_ShopArt.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerState.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "Lobby/TN_GeneralBriefing.h"
#include "Lobby/TN_ShopKeeper.h"
#include "Player/MP_GamePlayerController.h"
#include "UI/Shop/TN_ShopWidgets.h"

namespace TNBriefingUI
{
	const FMargin CardBoxMargin(0.16f, 0.2f, 0.16f, 0.34f);
	const FMargin RibbonBoxMargin(0.14f, 0.f, 0.14f, 0.f);
	const FMargin PillBoxMargin(0.4f, 0.f, 0.4f, 0.f);
	const FMargin TagBoxMargin(0.2f, 0.f, 0.2f, 0.f);
	const FMargin BubbleBoxMargin(0.26f, 0.3f, 0.18f, 0.45f);
	const TCHAR* const ControlsFolder = TEXT("/Game/Blueprints/Gameplay/Controls/");

	template <typename T>
	T* New(UWidgetTree* Tree)
	{
		return Tree->ConstructWidget<T>(T::StaticClass());
	}

	UTextBlock* Label(UWidgetTree* Tree, const FText& Content, FName Weight, int32 FontSize, const FLinearColor& Color, bool bOutline)
	{
		UTextBlock* Out = New<UTextBlock>(Tree);
		Out->SetText(Content);
		TNHUDStyle::StyleText(Out, Weight, FontSize, Color, bOutline);
		if (!bOutline) { Out->SetShadowColorAndOpacity(FLinearColor::Transparent); }
		return Out;
	}

	FSlateBrush BoxBrush(UTexture2D* Tex, const FMargin& Margin)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Tex);
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.Margin = Margin;
		if (Tex) { Brush.ImageSize = FVector2D(Tex->GetSizeX(), Tex->GetSizeY()); }
		return Brush;
	}

	UBorder* Framed(UWidgetTree* Tree, UTexture2D* Tex, const FMargin& Margin, UWidget* Content, const FMargin& Padding)
	{
		UBorder* Out = New<UBorder>(Tree);
		Out->SetBrush(BoxBrush(Tex, Margin));
		Out->SetPadding(Padding);
		Out->SetHorizontalAlignment(HAlign_Fill);
		Out->SetVerticalAlignment(VAlign_Fill);
		if (Content) { Out->SetContent(Content); }
		return Out;
	}

	USizeBox* Sized(UWidgetTree* Tree, UWidget* Content, float W, float H)
	{
		USizeBox* Out = New<USizeBox>(Tree);
		if (W > 0.f) { Out->SetWidthOverride(W); }
		if (H > 0.f) { Out->SetHeightOverride(H); }
		if (Content) { Out->SetContent(Content); }
		return Out;
	}

	void Pin(UCanvasPanel* Canvas, UWidget* W, const FVector2D& Anchor, const FVector2D& Offset)
	{
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(W);
		CanvasSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		CanvasSlot->SetAlignment(Anchor);
		CanvasSlot->SetPosition(Offset);
		CanvasSlot->SetAutoSize(true);
	}

	void AddH(UHorizontalBox* Box, UWidget* W, const FMargin& Padding = FMargin(0.f), EVerticalAlignment V = VAlign_Center)
	{
		UHorizontalBoxSlot* HSlot = Box->AddChildToHorizontalBox(W);
		HSlot->SetPadding(Padding);
		HSlot->SetVerticalAlignment(V);
	}

	void AddV(UVerticalBox* Box, UWidget* W, const FMargin& Padding = FMargin(0.f), EHorizontalAlignment H = HAlign_Fill)
	{
		UVerticalBoxSlot* VSlot = Box->AddChildToVerticalBox(W);
		VSlot->SetPadding(Padding);
		VSlot->SetHorizontalAlignment(H);
	}

	bool IsKey(const FKey& Key, std::initializer_list<FKey> Options)
	{
		for (const FKey& Option : Options) { if (Key == Option) { return true; } }
		return false;
	}

	FString Action(const TCHAR* Name)
	{
		return FString::Printf(TEXT("%s%s.%s"), ControlsFolder, Name, Name);
	}

	/** Nombre corto y en castellano de una tecla (las raras, como las da el motor). */
	FString KeyLabel(const FKey& Key)
	{
		struct FNamed { FKey Key; const TCHAR* Name; };
		static const FNamed Names[] = {
			{ EKeys::SpaceBar, TEXT("Espacio") }, { EKeys::LeftShift, TEXT("Mayús") }, { EKeys::RightShift, TEXT("Mayús der.") },
			{ EKeys::LeftControl, TEXT("Ctrl") }, { EKeys::RightControl, TEXT("Ctrl der.") }, { EKeys::LeftAlt, TEXT("Alt") },
			{ EKeys::Mouse2D, TEXT("Ratón") }, { EKeys::MouseX, TEXT("Ratón") }, { EKeys::MouseY, TEXT("Ratón") },
			{ EKeys::LeftMouseButton, TEXT("Clic izq.") }, { EKeys::RightMouseButton, TEXT("Clic der.") }, { EKeys::MiddleMouseButton, TEXT("Clic rueda") },
			{ EKeys::MouseScrollUp, TEXT("Rueda") }, { EKeys::MouseScrollDown, TEXT("Rueda") }, { EKeys::MouseWheelAxis, TEXT("Rueda") },
			{ EKeys::Enter, TEXT("Intro") }, { EKeys::Escape, TEXT("Esc") }, { EKeys::Tab, TEXT("Tab") },
			{ EKeys::Up, TEXT("↑") }, { EKeys::Down, TEXT("↓") }, { EKeys::Left, TEXT("←") }, { EKeys::Right, TEXT("→") },
			{ EKeys::Gamepad_FaceButton_Bottom, TEXT("A") }, { EKeys::Gamepad_FaceButton_Right, TEXT("B") },
			{ EKeys::Gamepad_FaceButton_Left, TEXT("X") }, { EKeys::Gamepad_FaceButton_Top, TEXT("Y") },
			{ EKeys::Gamepad_LeftShoulder, TEXT("LB") }, { EKeys::Gamepad_RightShoulder, TEXT("RB") },
			{ EKeys::Gamepad_LeftTrigger, TEXT("LT") }, { EKeys::Gamepad_LeftTriggerAxis, TEXT("LT") },
			{ EKeys::Gamepad_RightTrigger, TEXT("RT") }, { EKeys::Gamepad_RightTriggerAxis, TEXT("RT") },
			{ EKeys::Gamepad_Left2D, TEXT("Stick izq.") }, { EKeys::Gamepad_LeftX, TEXT("Stick izq.") }, { EKeys::Gamepad_LeftY, TEXT("Stick izq.") },
			{ EKeys::Gamepad_Right2D, TEXT("Stick der.") }, { EKeys::Gamepad_RightX, TEXT("Stick der.") }, { EKeys::Gamepad_RightY, TEXT("Stick der.") },
			{ EKeys::Gamepad_LeftThumbstick, TEXT("L3") }, { EKeys::Gamepad_RightThumbstick, TEXT("R3") },
			{ EKeys::Gamepad_DPad_Up, TEXT("Cruceta ↑") }, { EKeys::Gamepad_DPad_Down, TEXT("Cruceta ↓") },
			{ EKeys::Gamepad_DPad_Left, TEXT("Cruceta ←") }, { EKeys::Gamepad_DPad_Right, TEXT("Cruceta →") },
			{ EKeys::Gamepad_Special_Right, TEXT("Start") }, { EKeys::Gamepad_Special_Left, TEXT("Select") },
		};
		for (const FNamed& Named : Names)
		{
			if (Named.Key == Key) { return Named.Name; }
		}
		return Key.GetDisplayName(false).ToString();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Construcción
// ─────────────────────────────────────────────────────────────────────────────

void UTN_BriefingWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	BuildTree();
}

void UTN_BriefingWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// La radio del puesto baja mientras el general habla.
	ATN_ShopKeeper::SetRadiosDucked(GetWorld(), true);
	SetKeyboardFocus();
}

void UTN_BriefingWidget::NativeDestruct()
{
	ATN_ShopKeeper::SetRadiosDucked(GetWorld(), false);
	Super::NativeDestruct();
}

void UTN_BriefingWidget::BuildTree()
{
	using namespace TNBriefingUI;
	if (!WidgetTree || WidgetTree->RootWidget) { return; }
	UWidgetTree* Tree = WidgetTree;
	UCanvasPanel* Canvas = New<UCanvasPanel>(Tree);
	Tree->RootWidget = Canvas;

	// Velo azul marino sobre el juego.
	UImage* Veil = New<UImage>(Tree);
	Veil->SetColorAndOpacity(TNHUDArt::Hex(0x0A1C38, 0.66f));
	if (UCanvasPanelSlot* VeilSlot = Canvas->AddChildToCanvas(Veil))
	{
		VeilSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		VeilSlot->SetOffsets(FMargin(0.f));
	}

	// Título en cinta coral.
	TitleText = Label(Tree, NSLOCTEXT("Tortunabo", "BriefingTitle", "CUARTEL GENERAL"), TEXT("Black"), 34, TNHUDArt::Cream, true);
	UBorder* Title = Framed(Tree, TNHUDArt::RibbonTexture(), RibbonBoxMargin, TitleText, FMargin(70.f, 8.f, 70.f, 14.f));
	Title->SetHorizontalAlignment(HAlign_Center);
	Pin(Canvas, Title, FVector2D(0.5f, 0.f), FVector2D(0.f, 26.f));

	// Izquierda: el general y su bocadillo.
	UVerticalBox* Left = New<UVerticalBox>(Tree);
	{
		UImage* Face = New<UImage>(Tree);
		FSlateBrush FaceBrush;
		FaceBrush.SetResourceObject(TNHUDFaces::TurtleFace(ETNTurtleFace::Happy));
		FaceBrush.ImageSize = FVector2D(170.f, 170.f);
		Face->SetBrush(FaceBrush);
		AddV(Left, Face, FMargin(0.f), HAlign_Center);
		NameText = Label(Tree, NSLOCTEXT("Tortunabo", "GeneralName", "General Galápago"), TEXT("Bold"), 19, TNHUDArt::Cream, true);
		UBorder* NameRibbon = Framed(Tree, TNHUDArt::RibbonTexture(), RibbonBoxMargin, NameText, FMargin(34.f, 5.f, 34.f, 10.f));
		NameRibbon->SetHorizontalAlignment(HAlign_Center);
		AddV(Left, NameRibbon, FMargin(0.f, -16.f, 0.f, 10.f), HAlign_Center);
		DialogText = Label(Tree, FText::GetEmpty(), TEXT("Bold"), 21, TNHUDArt::Ink, false);
		DialogText->SetAutoWrapText(true);
		USizeBox* DialogSize = Sized(Tree, DialogText, 420.f, 0.f);
		DialogSize->SetMinDesiredHeight(150.f);
		AddV(Left, Framed(Tree, TNHUDArt::ChatBubbleTexture(), BubbleBoxMargin, DialogSize, FMargin(40.f, 18.f, 26.f, 32.f)), FMargin(0.f), HAlign_Center);
	}
	Pin(Canvas, Left, FVector2D(0.04f, 0.52f), FVector2D::ZeroVector);

	// Derecha: pestañas, página y botón de cerrar.
	UVerticalBox* Right = New<UVerticalBox>(Tree);
	{
		UHorizontalBox* TabRow = New<UHorizontalBox>(Tree);
		const FText TabNames[] = {
			NSLOCTEXT("Tortunabo", "BriefingTabHowTo", "CÓMO SE JUEGA"),
			NSLOCTEXT("Tortunabo", "BriefingTabModes", "MODOS DE JUEGO"),
			NSLOCTEXT("Tortunabo", "BriefingTabRules", "REGLAS"),
			NSLOCTEXT("Tortunabo", "BriefingTabControls", "CONTROLES"),
		};
		const int32 NumTabs = UE_ARRAY_COUNT(TabNames);
		for (int32 i = 0; i < NumTabs; ++i)
		{
			UTN_ShopButton* TabButton = CreateWidget<UTN_ShopButton>(this, UTN_ShopButton::StaticClass());
			TabButton->Setup(TabNames[i], TNShopArt::Pill(0x2A5A92, 0x173A66), TNHUDArt::Cream, 19, FVector2D(240.f, 56.f),
				[this, i]() { ShowTab(i); });
			AddH(TabRow, TabButton, FMargin(0.f, 0.f, 10.f, 0.f));
			Tabs.Add(TabButton);
		}
		AddV(Right, TabRow, FMargin(10.f, 0.f, 0.f, 8.f), HAlign_Left);

		Page = New<UVerticalBox>(Tree);
		Scroll = New<UScrollBox>(Tree);
		Scroll->AddChild(Page);
		AddV(Right, Framed(Tree, TNHUDArt::CardTexture(), CardBoxMargin, Sized(Tree, Scroll, 980.f, 540.f), FMargin(30.f, 26.f, 30.f, 44.f)),
			FMargin(0.f), HAlign_Left);

		UHorizontalBox* Bottom = New<UHorizontalBox>(Tree);
		UTN_ShopButton* OkButton = CreateWidget<UTN_ShopButton>(this, UTN_ShopButton::StaticClass());
		OkButton->Setup(NSLOCTEXT("Tortunabo", "BriefingOk", "¡ENTENDIDO!"), TNShopArt::Pill(0xFFD95E, 0xF2A93B), TNHUDArt::Ink, 24, FVector2D(320.f, 66.f),
			[this]() { Close(); });
		AddH(Bottom, OkButton, FMargin(0.f, 0.f, 20.f, 0.f));
		AddH(Bottom, Label(Tree, NSLOCTEXT("Tortunabo", "BriefingKeys", "Q/E o flechas: pestaña · ↑/↓: desplazar · Esc: salir"),
			TEXT("Regular"), 15, TNHUDArt::SeaLight, true));
		AddV(Right, Bottom, FMargin(10.f, 14.f, 0.f, 0.f), HAlign_Left);
	}
	Pin(Canvas, Right, FVector2D(0.97f, 0.54f), FVector2D::ZeroVector);
}

void UTN_BriefingWidget::SetGeneral(ATN_GeneralBriefing* InGeneral)
{
	General = InGeneral;
	if (InGeneral)
	{
		if (TitleText) { TitleText->SetText(FText::FromString(InGeneral->GetHeadquartersName().ToString().ToUpper())); }
		if (NameText) { NameText->SetText(InGeneral->GetGeneralName()); }
	}
	ShowTab(0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Páginas
// ─────────────────────────────────────────────────────────────────────────────

void UTN_BriefingWidget::AddHeading(const FText& Text)
{
	UTextBlock* Heading = TNBriefingUI::Label(WidgetTree, Text, TEXT("Black"), 23, TNHUDArt::CoralDeep, false);
	TNBriefingUI::AddV(Page, Heading, FMargin(0.f, Page->GetChildrenCount() > 0 ? 14.f : 0.f, 0.f, 4.f));
}

void UTN_BriefingWidget::AddParagraph(const FText& Text)
{
	UTextBlock* Paragraph = TNBriefingUI::Label(WidgetTree, Text, TEXT("Regular"), 19, TNHUDArt::Ink, false);
	Paragraph->SetAutoWrapText(true);
	TNBriefingUI::AddV(Page, Paragraph, FMargin(0.f, 0.f, 8.f, 6.f));
}

void UTN_BriefingWidget::KeysFor(const TArray<FString>& ActionPaths, TArray<FString>& OutKeyboard, TArray<FString>& OutGamepad) const
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Input)
	{
		return;
	}
	for (const FString& Path : ActionPaths)
	{
		const UInputAction* InputAction = LoadObject<UInputAction>(nullptr, *Path);
		if (!InputAction)
		{
			continue;
		}
		for (const FKey& Key : Input->QueryKeysMappedToAction(InputAction))
		{
			if (!Key.IsValid())
			{
				continue;
			}
			(Key.IsGamepadKey() ? OutGamepad : OutKeyboard).AddUnique(TNBriefingUI::KeyLabel(Key));
		}
	}
}

void UTN_BriefingWidget::AddControlRow(const FText& ActionName, const TArray<FString>& ActionPaths)
{
	using namespace TNBriefingUI;
	UWidgetTree* Tree = WidgetTree;
	TArray<FString> Keyboard, Gamepad;
	KeysFor(ActionPaths, Keyboard, Gamepad);

	UHorizontalBox* Row = New<UHorizontalBox>(Tree);
	UTextBlock* Name = Label(Tree, ActionName, TEXT("Bold"), 19, TNHUDArt::Ink, false);
	Name->SetAutoWrapText(true);
	AddH(Row, Sized(Tree, Name, 330.f, 0.f), FMargin(0.f, 0.f, 12.f, 0.f));
	if (Keyboard.Num() == 0 && Gamepad.Num() == 0)
	{
		AddH(Row, Label(Tree, NSLOCTEXT("Tortunabo", "BriefingNoKey", "— sin tecla —"), TEXT("Regular"), 17, TNHUDArt::WetSand, false));
	}
	for (const FString& KeyName : Keyboard)
	{
		UTextBlock* KeyText = Label(Tree, FText::FromString(KeyName), TEXT("Bold"), 16, TNHUDArt::Ink, false);
		AddH(Row, Framed(Tree, TNHUDArt::SandTagTexture(), TagBoxMargin, KeyText, FMargin(16.f, 4.f, 16.f, 8.f)), FMargin(0.f, 0.f, 6.f, 0.f));
	}
	for (const FString& KeyName : Gamepad)
	{
		UTextBlock* KeyText = Label(Tree, FText::FromString(KeyName), TEXT("Bold"), 15, TNHUDArt::Cream, true);
		AddH(Row, Framed(Tree, TNShopArt::Pill(0x3B6EA8, 0x1D3F6E), PillBoxMargin, KeyText, FMargin(18.f, 5.f, 18.f, 9.f)), FMargin(0.f, 0.f, 6.f, 0.f));
	}
	AddV(Page, Row, FMargin(0.f, 3.f, 0.f, 3.f));
}

void UTN_BriefingWidget::ShowTab(int32 Index)
{
	using TNBriefingUI::Action;
	Tab = (Index % 4 + 4) % 4;
	for (int32 i = 0; i < Tabs.Num(); ++i)
	{
		Tabs[i]->SetArt(i == Tab ? TNShopArt::Pill(0xFF8A70, 0xD9432F) : TNShopArt::Pill(0x2A5A92, 0x173A66));
	}
	if (!Page)
	{
		return;
	}
	Page->ClearChildren();

	const APlayerState* PS = GetOwningPlayer() ? GetOwningPlayer()->PlayerState : nullptr;
	const FText Who = FText::FromString(PS ? PS->GetPlayerName() : FString(TEXT("recluta")));

	switch (Tab)
	{
	case 0:
		Say(FText::Format(NSLOCTEXT("Tortunabo", "BriefingSayHowTo",
			"¡Firmes, {0}! Soy el General Galápago. De este cuartel se sale hacia el mar... y se sale sabiendo."), Who));
		AddHeading(NSLOCTEXT("Tortunabo", "BriefingGoalH", "El objetivo"));
		AddParagraph(NSLOCTEXT("Tortunabo", "BriefingGoal",
			"Acabáis de salir del huevo y el mar os espera. El camino cruza selva, playa, desierto, volcán, rocas y manglar: seguidlo hasta la meta, el arco de neumático gigante junto a la orilla, y ¡al agua!"));
		AddHeading(NSLOCTEXT("Tortunabo", "BriefingMoveH", "Moverse"));
		AddParagraph(NSLOCTEXT("Tortunabo", "BriefingMove1",
			"•  Anda y esprinta (el sprint gasta aliento: mira el salvavidas del HUD). Salta y, en el aire, pulsa saltar otra vez para el panzazo: te lanzas en plancha y cruzas huecos que andando no se cruzan."));
		AddParagraph(NSLOCTEXT("Tortunabo", "BriefingMove2",
			"•  En el agua se nada. Si saltas nadando, das un impulso para subir a las orillas y a las isletas."));
		AddHeading(NSLOCTEXT("Tortunabo", "BriefingShellH", "El caparazón"));
		AddParagraph(NSLOCTEXT("Tortunabo", "BriefingShell1",
			"•  Métete en tu caparazón: caes al suelo y ruedas, resbalas y rebotas con física de verdad (¡y los demás te pueden empujar!). Pulsa otra vez para salir."));
		AddParagraph(NSLOCTEXT("Tortunabo", "BriefingShell2",
			"•  Con Interactuar coges a una tortuga metida en su caparazón (o noqueada) y la lanzas hacia donde miras: sale dando volteretas y no puede salir hasta que se para. Si te llevan a ti, muévete sin parar dos segundos para escaparte."));
		AddHeading(NSLOCTEXT("Tortunabo", "BriefingDangerH", "Cuidado"));
		AddParagraph(NSLOCTEXT("Tortunabo", "BriefingDanger1",
			"•  Plátanos y golpes te noquean: te quedas un momento en el suelo con los pajaritos dando vueltas y luego te levantas."));
		AddParagraph(NSLOCTEXT("Tortunabo", "BriefingDanger2",
			"•  Si caes más de 5 m te haces bola tú solo; si caes de muy alto, te rompes. Géiseres, toboganes y agua no cuentan."));
		AddParagraph(NSLOCTEXT("Tortunabo", "BriefingDanger3",
			"•  La fauna del agua muerde. Las pilas de huevos del camino son vuestros puntos de reaparición: pasad por ellas."));
		break;
	case 1:
		Say(NSLOCTEXT("Tortunabo", "BriefingSayModes",
			"Cuatro formas de llegar a la playa. Elegid bien en los selectores de la salida, que luego no hay vuelta atrás."));
		AddHeading(NSLOCTEXT("Tortunabo", "BriefingCoopH", "Cooperativo (de 1 a 4 tortugas)"));
		AddParagraph(NSLOCTEXT("Tortunabo", "BriefingCoop",
			"Todo el equipo tiene que llegar a la meta. Una tormenta avanza por el camino detrás de vosotros y nunca va más rápido que una tortuga andando: si os quedáis atrás, os alcanza. Es el mapa más largo."));
		AddHeading(NSLOCTEXT("Tortunabo", "BriefingRaceH", "Carrera (de 1 a 4)"));
		AddParagraph(NSLOCTEXT("Tortunabo", "BriefingRace",
			"Todos contra todos: el primero en llegar a la meta gana la ronda, y la partida es para quien gane tres. Mapas más cortos y 15 minutos por ronda."));
		AddHeading(NSLOCTEXT("Tortunabo", "Briefing2v2H", "2 vs 2 (exactamente 4)"));
		AddParagraph(NSLOCTEXT("Tortunabo", "Briefing2v2",
			"Por parejas: gana la ronda la pareja cuyos dos miembros llegan antes. Hay muros que solo se superan lanzando al compañero (o bajando la rampa con el interruptor) y compuertas para sabotear a la otra pareja. Las parejas cambian cada ronda; si no sois cuatro, se juega Carrera."));
		AddHeading(NSLOCTEXT("Tortunabo", "BriefingClassicH", "Clásico"));
		AddParagraph(NSLOCTEXT("Tortunabo", "BriefingClassic", "El recorrido de siempre, por tramos, para los veteranos del cuartel."));
		AddHeading(NSLOCTEXT("Tortunabo", "BriefingPickH", "Cómo se elige"));
		AddParagraph(NSLOCTEXT("Tortunabo", "BriefingPick",
			"Con los dos selectores junto a la salida: uno cambia el modo y el otro la dificultad (Fácil, Normal o Difícil). La dificultad cambia el tamaño del mapa, los cruces colosales, los huecos y lo rápida que va la tormenta."));
		break;
	case 2:
		Say(NSLOCTEXT("Tortunabo", "BriefingSayRules", "Las normas del cuartel no se discuten. Bueno, se pueden discutir... pero se pierde."));
		AddHeading(NSLOCTEXT("Tortunabo", "BriefingStartH", "La salida"));
		AddParagraph(NSLOCTEXT("Tortunabo", "BriefingStart",
			"•  Cuando estéis listos, id a la sala de espera junto a la gran puerta del castillo y meteos cada uno en un huevo. Con todos dentro empieza la cuenta atrás, la puerta se abre... ¡y a la playa!"));
		AddHeading(NSLOCTEXT("Tortunabo", "BriefingRespawnH", "Reaparición"));
		AddParagraph(NSLOCTEXT("Tortunabo", "BriefingRespawn1",
			"•  Si caes, vuelves a la última pila de huevos alcanzada que quede por delante de la tormenta: en Cooperativo, la más lejana del equipo; en Carrera y 2 vs 2, la tuya."));
		AddParagraph(NSLOCTEXT("Tortunabo", "BriefingRespawn2", "•  Sin pila válida te quedas en el suelo y un compañero tiene que rescatarte."));
		AddHeading(NSLOCTEXT("Tortunabo", "BriefingTimeH", "Tiempo"));
		AddParagraph(NSLOCTEXT("Tortunabo", "BriefingTime",
			"•  En Carrera y 2 vs 2 cada ronda dura como mucho 15 minutos: si se acaba, gana el más adelantado en el camino."));
		AddHeading(NSLOCTEXT("Tortunabo", "BriefingFairH", "Juego limpio"));
		AddParagraph(NSLOCTEXT("Tortunabo", "BriefingFair",
			"•  Se puede coger y lanzar a cualquiera que esté metido en su caparazón, también a los rivales. Lo que no se puede es quedarse en la salida molestando: el general lo ve todo."));
		break;
	default:
		Say(NSLOCTEXT("Tortunabo", "BriefingSayControls", "Estas son tus teclas de verdad, recluta. Apréndetelas antes de salir del huevo."));
		AddHeading(NSLOCTEXT("Tortunabo", "BriefingKeysH", "Tus teclas"));
		AddParagraph(NSLOCTEXT("Tortunabo", "BriefingKeysNote", "Leídas de tu configuración actual. En azul, las del mando."));
		AddControlRow(NSLOCTEXT("Tortunabo", "CtlMove", "Moverse"), { Action(TEXT("IA_Move")) });
		AddControlRow(NSLOCTEXT("Tortunabo", "CtlLook", "Mirar"), { Action(TEXT("IA_Look")) });
		AddControlRow(NSLOCTEXT("Tortunabo", "CtlJump", "Saltar (en el aire, otra vez: panzazo)"), { Action(TEXT("IA_Jump")) });
		AddControlRow(NSLOCTEXT("Tortunabo", "CtlSprint", "Esprintar"), { Action(TEXT("IA_Sprint")) });
		AddControlRow(NSLOCTEXT("Tortunabo", "CtlShell", "Meterse o salir del caparazón"), { Action(TEXT("IA_Shell")) });
		AddControlRow(NSLOCTEXT("Tortunabo", "CtlInteract", "Interactuar, coger y lanzar"), { Action(TEXT("IA_Interact")) });
		AddControlRow(NSLOCTEXT("Tortunabo", "CtlDrop", "Soltar el objeto (o a quien llevas)"), { Action(TEXT("IA_DropItem")) });
		AddControlRow(NSLOCTEXT("Tortunabo", "CtlRotate", "Cambiar de objeto"), { Action(TEXT("IA_RotateInventory")) });
		AddControlRow(NSLOCTEXT("Tortunabo", "CtlEmoteWheel", "Rueda de emotes"), { Action(TEXT("IA_OpenEmoteWheel")) });
		AddControlRow(NSLOCTEXT("Tortunabo", "CtlChatWheel", "Chat rápido"), { Action(TEXT("IA_OpenChatWheel")) });
		{
			TArray<FString> EmotePaths;
			for (int32 i = 1; i <= 9; ++i) { EmotePaths.Add(Action(*FString::Printf(TEXT("IA_Emote%d"), i))); }
			EmotePaths.Add(Action(TEXT("IA_Emote")));
			AddControlRow(NSLOCTEXT("Tortunabo", "CtlEmotes", "Emotes directos"), EmotePaths);
		}
		break;
	}
	if (Scroll) { Scroll->ScrollToStart(); }
}

void UTN_BriefingWidget::Say(const FText& Line)
{
	FullLine = Line.ToString();
	Reveal = 0.f;
	if (DialogText) { DialogText->SetText(FText::GetEmpty()); }
}

void UTN_BriefingWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// El general habla letra a letra.
	if (DialogText && Reveal < FullLine.Len())
	{
		Reveal = FMath::Min<float>(FullLine.Len(), Reveal + InDeltaTime * 70.f);
		DialogText->SetText(FText::FromString(FullLine.Left(FMath::CeilToInt(Reveal))));
	}
}

void UTN_BriefingWidget::Close()
{
	if (AMP_GamePlayerController* PC = Cast<AMP_GamePlayerController>(GetOwningPlayer())) { PC->CloseShopUI(); }
	else { RemoveFromParent(); }
}

FReply UTN_BriefingWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	using TNBriefingUI::IsKey;
	const FKey Key = InKeyEvent.GetKey();
	if (IsKey(Key, { EKeys::Q, EKeys::Left, EKeys::A, EKeys::Gamepad_LeftShoulder, EKeys::Gamepad_DPad_Left })) { ShowTab(Tab - 1); return FReply::Handled(); }
	if (IsKey(Key, { EKeys::E, EKeys::Tab, EKeys::Right, EKeys::D, EKeys::Gamepad_RightShoulder, EKeys::Gamepad_DPad_Right })) { ShowTab(Tab + 1); return FReply::Handled(); }
	if (IsKey(Key, { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four }))
	{
		ShowTab(Key == EKeys::One ? 0 : Key == EKeys::Two ? 1 : Key == EKeys::Three ? 2 : 3);
		return FReply::Handled();
	}
	if (Scroll && IsKey(Key, { EKeys::Up, EKeys::W, EKeys::Gamepad_DPad_Up }))
	{
		Scroll->SetScrollOffset(FMath::Max(0.f, Scroll->GetScrollOffset() - 90.f));
		return FReply::Handled();
	}
	if (Scroll && IsKey(Key, { EKeys::Down, EKeys::S, EKeys::Gamepad_DPad_Down }))
	{
		Scroll->SetScrollOffset(FMath::Min(Scroll->GetScrollOffsetOfEnd(), Scroll->GetScrollOffset() + 90.f));
		return FReply::Handled();
	}
	if (IsKey(Key, { EKeys::Escape, EKeys::Enter, EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_Special_Right }))
	{
		Close();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
