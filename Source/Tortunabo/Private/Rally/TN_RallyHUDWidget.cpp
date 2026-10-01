#include "Rally/TN_RallyHUDWidget.h"

#include "../UI/Race/TN_RaceUIKit.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Rally/TN_RallyLogic.h"
#include "Rally/TN_RallyPlayerState.h"
#include "Rally/TN_RallyVehicle.h"
#include "Vehicles/TN_Buggy.h"
#include "Vehicles/TN_BuggyGunnerPawn.h"
#include "Vehicles/TN_BuggyTurretComponent.h"

namespace TNRallyHUD
{
	/** Cuántos segundos se ve el semáforo en verde tras la salida. */
	constexpr double GreenHoldSeconds = 1.5;
	/** Último segundo de tinta: fundido. */
	constexpr float InkFadeSeconds = 1.f;
	constexpr float TextRefreshSeconds = 0.1f;

	const FLinearColor LightOff(0.06f, 0.07f, 0.09f, 0.9f);
	const FLinearColor LightRed(0.95f, 0.16f, 0.12f, 1.f);
	const FLinearColor LightGreen(0.2f, 0.9f, 0.35f, 1.f);
	const FLinearColor InkColor(0.03f, 0.02f, 0.06f, 1.f);

	FText PlaceOfTotal(int32 Place, int32 Total)
	{
		return FText::Format(NSLOCTEXT("Rally", "PlaceOfTotal", "{0}.º / {1}"), TNLocText::Int(Place), TNLocText::Int(Total));
	}

	FText RaceTime(float Seconds)
	{
		const float Safe = FMath::Max(0.f, Seconds);
		const int32 Minutes = FMath::FloorToInt(Safe / 60.f);
		FNumberFormattingOptions Options;
		Options.UseGrouping = false;
		Options.MinimumIntegralDigits = 2;
		Options.MinimumFractionalDigits = 1;
		Options.MaximumFractionalDigits = 1;
		Options.RoundingMode = ERoundingMode::ToZero;
		return FText::Format(NSLOCTEXT("Rally", "RaceTime", "{0}:{1}"), TNLocText::Int(Minutes),
			FText::AsNumber(Safe - 60.f * Minutes, &Options));
	}

	FText AmmoName(ETNRallyAmmo Ammo)
	{
		switch (Ammo)
		{
		case ETNRallyAmmo::Coco: return NSLOCTEXT("Rally", "AmmoCoco", "Coco");
		case ETNRallyAmmo::Alga: return NSLOCTEXT("Rally", "AmmoAlga", "Alga");
		case ETNRallyAmmo::Burbuja: return NSLOCTEXT("Rally", "AmmoBurbuja", "Burbuja");
		case ETNRallyAmmo::Mortero: return NSLOCTEXT("Rally", "AmmoMortero", "Mortero");
		case ETNRallyAmmo::Tinta: return NSLOCTEXT("Rally", "AmmoTinta", "Tinta");
		default: return FText::GetEmpty();
		}
	}

	FText CrewName(const FTNRallyStanding& Entry)
	{
		TArray<FText> Names;
		if (Entry.Driver) { Names.Add(TNLocText::PlayerName(Entry.Driver->GetPlayerName())); }
		if (Entry.Gunner) { Names.Add(TNLocText::PlayerName(Entry.Gunner->GetPlayerName())); }
		return Names.Num() > 0 ? TNLocText::JoinList(Names) : NSLOCTEXT("Rally", "EmptyBuggy", "Buggy vacío");
	}

	int32 CeilSeconds(double Seconds)
	{
		return FMath::Max(0, FMath::CeilToInt(static_cast<float>(Seconds)));
	}

	void Show(UWidget* Widget, bool bVisible)
	{
		if (Widget)
		{
			Widget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}

void UTN_RallyHUDWidget::NativeOnInitialized()
{
	BuildTree();
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UTN_RallyHUDWidget::SetTurretHeat(float Heat01)
{
	TurretHeat = FMath::Clamp(Heat01, 0.f, 1.f);
	RefreshTurret();
}

void UTN_RallyHUDWidget::SetInkSeconds(float SecondsLeft)
{
	InkSeconds = FMath::Max(0.f, SecondsLeft);
}

const ATN_Buggy* UTN_RallyHUDWidget::FindLocalBuggy() const
{
	const APlayerController* Player = GetOwningPlayer();
	if (!Player)
	{
		return nullptr;
	}
	const APawn* Pawn = Player->GetPawn();
	if (const ATN_Buggy* Driven = Cast<ATN_Buggy>(Pawn))
	{
		return Driven;
	}
	if (const ATN_BuggyGunnerPawn* Gunner = Cast<ATN_BuggyGunnerPawn>(Pawn))
	{
		return Gunner->GetBuggy();
	}
	// Sin peón propio (reaparición, cambio de plaza): el buggy de su fila de puestos.
	const UWorld* World = GetWorld();
	const ATN_RallyGameState* RallyState = World ? World->GetGameState<ATN_RallyGameState>() : nullptr;
	const FTNRallyStanding* Mine = RallyState ? RallyState->FindStandingForPlayer(Player->GetPlayerState<ATN_RallyPlayerState>()) : nullptr;
	return Mine ? Cast<ATN_Buggy>(Mine->Vehicle) : nullptr;
}

void UTN_RallyHUDWidget::PullFromLocalBuggy()
{
	const ATN_Buggy* Buggy = FindLocalBuggy();
	const UTN_BuggyTurretComponent* Turret = Buggy ? Buggy->GetTurret() : nullptr;
	bTurretOverheated = Turret && Turret->IsOverheated();
	SpecialCharges = Turret ? Turret->GetSpecialCharges() : 0;
	SetTurretHeat(Turret ? Turret->GetHeat01() : 0.f);
	SetInkSeconds(Buggy ? Buggy->GetInkSecondsLeft() : 0.f);
}

void UTN_RallyHUDWidget::BuildTree()
{
	using namespace TNRaceUI;
	if (!WidgetTree || Canvas)
	{
		return;
	}
	UWidgetTree* Tree = WidgetTree;
	Canvas = Make<UCanvasPanel>(Tree, TEXT("RallyCanvas"));
	Tree->RootWidget = Canvas;

	// La tinta va la primera: tapa la carretera pero no los números del HUD.
	BuildInk();

	PlaceText = MakeText(Tree, FText::GetEmpty(), TEXT("Bold"), 56, TNHUDArt::Gold);
	Place(Canvas, PlaceText, FVector2D(0.f, 0.f), FVector2D(40.f, 24.f));
	LapText = MakeText(Tree, FText::GetEmpty(), TEXT("Bold"), 26, FLinearColor::White);
	Place(Canvas, LapText, FVector2D(0.f, 0.f), FVector2D(44.f, 100.f));

	SpeedText = MakeText(Tree, FText::GetEmpty(), TEXT("Bold"), 64, FLinearColor::White);
	Place(Canvas, SpeedText, FVector2D(1.f, 1.f), FVector2D(-48.f, -64.f));
	SpeedUnitText = MakeText(Tree, NSLOCTEXT("Rally", "SpeedUnit", "km/h"), TEXT("Regular"), 24, TNHUDStyle::TextDim);
	Place(Canvas, SpeedUnitText, FVector2D(1.f, 1.f), FVector2D(-48.f, -30.f));

	UHorizontalBox* Lights = Make<UHorizontalBox>(Tree);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UImage* Light = Make<UImage>(Tree);
		Light->SetBrush(TNHUDStyle::Rounded(FLinearColor::White, 32.f, FLinearColor(0.f, 0.f, 0.f, 0.6f), 3.f));
		Light->SetColorAndOpacity(TNRallyHUD::LightOff);
		if (UHorizontalBoxSlot* LightSlot = Lights->AddChildToHorizontalBox(MakeSize(Tree, Light, 64.f, 64.f)))
		{
			LightSlot->SetPadding(FMargin(8.f, 0.f));
		}
		SemaphoreLights.Add(Light);
	}
	UBorder* LightsPanel = Make<UBorder>(Tree);
	TNHUDStyle::StylePanel(LightsPanel, TNHUDStyle::Panel, 24.f, FMargin(14.f, 10.f));
	LightsPanel->SetContent(Lights);
	SemaphoreBox = LightsPanel;
	Place(Canvas, LightsPanel, FVector2D(0.5f, 0.f), FVector2D(0.f, 36.f));

	CenterText = MakeText(Tree, FText::GetEmpty(), TEXT("Bold"), 110, FLinearColor::White);
	Place(Canvas, CenterText, FVector2D(0.5f, 0.3f), FVector2D::ZeroVector);
	StatusText = MakeText(Tree, FText::GetEmpty(), TEXT("Bold"), 30, TNHUDArt::SandLight);
	Place(Canvas, StatusText, FVector2D(0.5f, 0.f), FVector2D(0.f, 130.f));
	WrongWayText = MakeText(Tree, NSLOCTEXT("Rally", "WrongWay", "¡CONTRAMANO!"), TEXT("Bold"), 64, TNHUDArt::CoralC);
	Place(Canvas, WrongWayText, FVector2D(0.5f, 0.42f), FVector2D::ZeroVector);
	RespawnText = MakeText(Tree, FText::GetEmpty(), TEXT("Bold"), 36, TNHUDArt::SandC);
	Place(Canvas, RespawnText, FVector2D(0.5f, 0.56f), FVector2D::ZeroVector);
	Crosshair = MakeText(Tree, TNLocText::Literal(TEXT("+")), TEXT("Bold"), 48, FLinearColor::White);
	Place(Canvas, Crosshair, FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);

	AmmoText = MakeText(Tree, FText::GetEmpty(), TEXT("Bold"), 28, TNHUDArt::Foam);
	Place(Canvas, AmmoText, FVector2D(0.f, 1.f), FVector2D(40.f, -84.f));
	HeatLabel = MakeText(Tree, NSLOCTEXT("Rally", "TurretHeat", "Torreta"), TEXT("Regular"), 22, TNHUDStyle::TextDim);
	Place(Canvas, HeatLabel, FVector2D(0.f, 1.f), FVector2D(40.f, -52.f));
	HeatBar = Make<UProgressBar>(Tree);
	HeatBar->SetWidgetStyle(TNHUDStyle::Bar(9.f));
	HeatBar->SetPercent(0.f);
	Place(Canvas, MakeSize(Tree, HeatBar, 260.f, 18.f), FVector2D(0.f, 1.f), FVector2D(40.f, -28.f));

	BuildResults();

	for (UWidget* Hidden : TArray<UWidget*>{ WrongWayText, RespawnText, Crosshair, AmmoText, CenterText, StatusText, ResultsPanel })
	{
		TNRallyHUD::Show(Hidden, false);
	}
	RefreshTurret();
}

void UTN_RallyHUDWidget::BuildInk()
{
	// Manchas de tinta (posición en fracción de pantalla y tamaño en px): se ven mientras el buggy tiene tinta.
	struct FSplat { FVector2D Anchor; float Size; };
	const FSplat Splats[] = {
		{ FVector2D(0.3f, 0.35f), 420.f }, { FVector2D(0.62f, 0.3f), 360.f }, { FVector2D(0.5f, 0.6f), 480.f },
		{ FVector2D(0.18f, 0.7f), 300.f }, { FVector2D(0.8f, 0.62f), 340.f }, { FVector2D(0.45f, 0.18f), 260.f } };
	for (const FSplat& Splat : Splats)
	{
		UImage* Ink = TNRaceUI::Make<UImage>(WidgetTree);
		Ink->SetBrush(TNHUDStyle::Rounded(TNRallyHUD::InkColor, Splat.Size * 0.5f));
		Ink->SetRenderOpacity(0.f);
		UWidget* Sized = TNRaceUI::MakeSize(WidgetTree, Ink, Splat.Size, Splat.Size * 0.85f);
		TNRaceUI::Place(Canvas, Sized, Splat.Anchor, FVector2D::ZeroVector);
		InkSplats.Add(Ink);
	}
}

void UTN_RallyHUDWidget::BuildResults()
{
	using namespace TNRaceUI;
	UWidgetTree* Tree = WidgetTree;
	UVerticalBox* Content = Make<UVerticalBox>(Tree);
	UTextBlock* Title = MakeText(Tree, NSLOCTEXT("Rally", "ResultsTitle", "Resultados"), TEXT("Bold"), 44, TNHUDArt::Gold);
	if (UVerticalBoxSlot* TitleSlot = Content->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
	}
	ResultsRows = Make<UVerticalBox>(Tree);
	Content->AddChildToVerticalBox(ResultsRows);
	ResultsFooter = MakeText(Tree, FText::GetEmpty(), TEXT("Regular"), 24, TNHUDStyle::TextDim);
	if (UVerticalBoxSlot* FooterSlot = Content->AddChildToVerticalBox(ResultsFooter))
	{
		FooterSlot->SetHorizontalAlignment(HAlign_Center);
		FooterSlot->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
	}
	ResultsPanel = Make<UBorder>(Tree);
	TNHUDStyle::StylePanel(ResultsPanel, TNHUDStyle::Panel, 22.f, FMargin(40.f, 28.f));
	ResultsPanel->SetContent(Content);
	Place(Canvas, ResultsPanel, FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
}

void UTN_RallyHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	PullFromLocalBuggy();
	const float InkOpacity = FMath::Clamp(InkSeconds / TNRallyHUD::InkFadeSeconds, 0.f, 1.f) * 0.94f;
	for (UImage* Ink : InkSplats)
	{
		Ink->SetRenderOpacity(InkOpacity);
	}

	const UWorld* World = GetWorld();
	const ATN_RallyGameState* RallyState = World ? World->GetGameState<ATN_RallyGameState>() : nullptr;
	if (!RallyState)
	{
		return;
	}
	RefreshSemaphore(*RallyState, RallyState->GetServerWorldTimeSeconds());
	TextAccumulator += InDeltaTime;
	if (TextAccumulator >= TNRallyHUD::TextRefreshSeconds)
	{
		TextAccumulator = 0.f;
		Refresh(*RallyState);
	}
}

void UTN_RallyHUDWidget::Refresh(const ATN_RallyGameState& RallyState)
{
	using namespace TNRallyHUD;
	const double ServerTime = RallyState.GetServerWorldTimeSeconds();
	const APlayerController* Player = GetOwningPlayer();
	const ATN_RallyPlayerState* Me = Player ? Player->GetPlayerState<ATN_RallyPlayerState>() : nullptr;
	const FTNRallyStanding* Mine = RallyState.FindStandingForPlayer(Me);
	const ITN_RallyVehicle* Vehicle = Mine ? Cast<ITN_RallyVehicle>(Mine->Vehicle) : nullptr;
	const bool bRacing = RallyState.Phase == ETNRallyPhase::Racing || RallyState.Phase == ETNRallyPhase::Finishing;

	Show(PlaceText, Mine != nullptr);
	Show(LapText, Mine != nullptr);
	if (Mine)
	{
		PlaceText->SetText(PlaceOfTotal(Mine->Place, RallyState.Standings.Num()));
		const int32 GateTotal = RallyState.bCircuit ? RallyState.NumGates : FMath::Max(0, RallyState.NumGates - 1);
		const int32 GateShown = (RallyState.bCircuit && Mine->NextGate == 0) ? (Mine->Lap > 0 ? GateTotal : 0) : Mine->NextGate;
		const FText Gate = FText::Format(NSLOCTEXT("Rally", "GateOfTotal", "Puerta {0}/{1}"), TNLocText::Int(GateShown), TNLocText::Int(GateTotal));
		if (Mine->bFinished)
		{
			LapText->SetText(FText::Format(NSLOCTEXT("Rally", "FinishedTime", "¡Meta! {0}"), RaceTime(Mine->FinishSeconds)));
		}
		else if (RallyState.Laps > 1)
		{
			LapText->SetText(FText::Format(NSLOCTEXT("Rally", "LapAndGate", "Vuelta {0}/{1} · {2}"),
				TNLocText::Int(FMath::Max(1, Mine->Lap)), TNLocText::Int(RallyState.Laps), Gate));
		}
		else
		{
			LapText->SetText(Gate);
		}
	}

	Show(SpeedText, Vehicle != nullptr);
	Show(SpeedUnitText, Vehicle != nullptr);
	if (Vehicle)
	{
		SpeedText->SetText(TNLocText::Int(FMath::RoundToInt(static_cast<float>(TNRally::CmsToKmh(FMath::Abs(Vehicle->GetForwardSpeedCms()))))));
	}
	const ETNRallyAmmo Special = Vehicle ? Vehicle->GetSpecialAmmo() : ETNRallyAmmo::None;
	Show(AmmoText, Special != ETNRallyAmmo::None && Special != ETNRallyAmmo::Coco);
	if (AmmoText->GetVisibility() != ESlateVisibility::Collapsed)
	{
		AmmoText->SetText(FText::Format(NSLOCTEXT("Rally", "SpecialAmmoCharges", "Munición: {0} ×{1}"), AmmoName(Special),
			TNLocText::Int(FMath::Max(0, SpecialCharges))));
	}
	HeatLabel->SetText(bTurretOverheated ? NSLOCTEXT("Rally", "TurretOverheated", "¡Torreta sobrecalentada!")
		: NSLOCTEXT("Rally", "TurretHeat", "Torreta"));
	Show(Crosshair, Me && Me->IsGunner() && RallyState.Phase != ETNRallyPhase::Results);
	Show(HeatBar ? HeatBar->GetParent() : nullptr, Mine != nullptr);
	Show(HeatLabel, Mine != nullptr);

	Show(WrongWayText, bRacing && Mine && Mine->bWrongWay && !Mine->bFinished);
	const double RespawnLeft = Mine ? Mine->RespawnEndServerTime - ServerTime : 0.0;
	Show(RespawnText, Mine && Mine->RespawnEndServerTime > 0.f && RespawnLeft > 0.0);
	if (RespawnLeft > 0.0)
	{
		RespawnText->SetText(FText::Format(NSLOCTEXT("Rally", "Respawning", "Reapareciendo… {0}"), TNLocText::Int(CeilSeconds(RespawnLeft))));
	}

	RefreshStatus(RallyState, ServerTime, Mine);
	RefreshResults(RallyState, ServerTime);
}

void UTN_RallyHUDWidget::RefreshSemaphore(const ATN_RallyGameState& RallyState, double ServerTime)
{
	using namespace TNRallyHUD;
	const double ToStart = RallyState.StartServerTime - ServerTime;
	const bool bCounting = RallyState.Phase == ETNRallyPhase::Countdown && ToStart > 0.0;
	const bool bGreen = (RallyState.Phase == ETNRallyPhase::Countdown && ToStart <= 0.0)
		|| (RallyState.Phase == ETNRallyPhase::Racing && -ToStart < GreenHoldSeconds);
	Show(SemaphoreBox, bCounting || bGreen);
	Show(CenterText, bCounting || bGreen);
	if (bCounting)
	{
		// Una luz roja más por segundo: 3 s → una, 2 s → dos, 1 s → tres.
		const int32 Lit = FMath::Clamp(3 - FMath::FloorToInt(static_cast<float>(ToStart)), 1, 3);
		for (int32 Index = 0; Index < SemaphoreLights.Num(); ++Index)
		{
			SemaphoreLights[Index]->SetColorAndOpacity(Index < Lit ? LightRed : LightOff);
		}
		CenterText->SetText(TNLocText::Int(CeilSeconds(ToStart)));
		CenterText->SetColorAndOpacity(FSlateColor(LightRed));
	}
	else if (bGreen)
	{
		for (UImage* Light : SemaphoreLights)
		{
			Light->SetColorAndOpacity(LightGreen);
		}
		CenterText->SetText(NSLOCTEXT("Rally", "Go", "¡YA!"));
		CenterText->SetColorAndOpacity(FSlateColor(LightGreen));
	}
}

void UTN_RallyHUDWidget::RefreshStatus(const ATN_RallyGameState& RallyState, double ServerTime, const FTNRallyStanding* Mine)
{
	using namespace TNRallyHUD;
	FText Status;
	const double PhaseLeft = RallyState.PhaseEndServerTime - ServerTime;
	switch (RallyState.Phase)
	{
	case ETNRallyPhase::Warmup:
		Status = RallyState.PhaseEndServerTime > 0.f
			? FText::Format(NSLOCTEXT("Rally", "WarmupStarts", "La carrera empieza en {0} s"), TNLocText::Int(CeilSeconds(PhaseLeft)))
			: NSLOCTEXT("Rally", "WarmupWaiting", "Esperando a las tortugas…");
		break;
	case ETNRallyPhase::Racing:
		Status = Mine ? FText::GetEmpty() : NSLOCTEXT("Rally", "Spectating", "Mirando la carrera");
		break;
	case ETNRallyPhase::Finishing:
		Status = FText::Format(NSLOCTEXT("Rally", "FinishCountdown", "Fin de la carrera en {0} s"), TNLocText::Int(CeilSeconds(PhaseLeft)));
		break;
	default:
		break;
	}
	Show(StatusText, !Status.IsEmpty());
	if (!Status.IsEmpty())
	{
		StatusText->SetText(Status);
		StatusText->SetColorAndOpacity(FSlateColor(RallyState.Phase == ETNRallyPhase::Finishing && PhaseLeft < 5.0 ? TNHUDArt::CoralC : TNHUDArt::SandLight));
	}
}

void UTN_RallyHUDWidget::RefreshResults(const ATN_RallyGameState& RallyState, double ServerTime)
{
	using namespace TNRallyHUD;
	const bool bResults = RallyState.Phase == ETNRallyPhase::Results;
	Show(ResultsPanel, bResults);
	if (!bResults)
	{
		ShownResultsHash = 0;
		return;
	}
	ResultsFooter->SetText(FText::Format(NSLOCTEXT("Rally", "NextRace", "Carrera nueva en {0} s"),
		TNLocText::Int(CeilSeconds(RallyState.PhaseEndServerTime - ServerTime))));

	uint32 Hash = 1;
	for (const FTNRallyStanding& Entry : RallyState.Standings)
	{
		Hash = HashCombine(Hash, GetTypeHash(Entry.TeamIndex));
		Hash = HashCombine(Hash, GetTypeHash(Entry.Place));
		Hash = HashCombine(Hash, GetTypeHash(Entry.FinishSeconds));
		Hash = HashCombine(Hash, GetTypeHash(Entry.Driver.Get()));
		Hash = HashCombine(Hash, GetTypeHash(Entry.Gunner.Get()));
	}
	if (Hash == ShownResultsHash)
	{
		return;
	}
	ShownResultsHash = Hash;
	ResultsRows->ClearChildren();
	const APlayerController* Player = GetOwningPlayer();
	const APlayerState* Me = Player ? Player->PlayerState : nullptr;
	for (const FTNRallyStanding& Entry : RallyState.Standings)
	{
		const FText Time = Entry.bFinished ? RaceTime(Entry.FinishSeconds) : NSLOCTEXT("Rally", "NotFinished", "Sin llegar");
		const FText Line = FText::Format(NSLOCTEXT("Rally", "ResultsRow", "{0}.º   {1}   {2}   {3} pts"),
			TNLocText::Int(Entry.Place), CrewName(Entry), Time, TNLocText::Int(Entry.Points));
		const bool bMine = Me && (Entry.Driver == Me || Entry.Gunner == Me);
		UTextBlock* Row = TNRaceUI::MakeText(WidgetTree, Line, TEXT("Bold"), 30, bMine ? TNHUDArt::Gold : FLinearColor::White);
		if (UVerticalBoxSlot* RowSlot = ResultsRows->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.f, 4.f));
		}
	}
}

void UTN_RallyHUDWidget::RefreshTurret()
{
	if (!HeatBar)
	{
		return;
	}
	HeatBar->SetPercent(TurretHeat);
	HeatBar->SetFillColorAndOpacity(FMath::Lerp(TNHUDStyle::Accent, TNHUDArt::CoralC, TurretHeat));
}
