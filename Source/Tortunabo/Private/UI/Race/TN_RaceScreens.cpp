#include "UI/Race/TN_RaceScreens.h"
#include "TN_RaceArt.h"
#include "../../Audio/TN_MatchMusicSubsystem.h"
#include "Audio/TN_MusicSynthComponent.h"
#include "Blueprint/UserWidget.h"
#include "Core/TN_CoopPlayerState.h"
#include "Core/TN_Log.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Multiplayer/MP_GameInstance.h"
#include "Stats/Stats.h"

// Con nombre (no anónimo): en la compilación por bloques (unity) los nombres de un espacio anónimo se ven en el resto
// del bloque.
namespace TNRaceScreensDetail
{
	/** Capas de la pantalla: por encima del HUD (4, 5 y 10) y por debajo de las ruedas (30) y los menús (40). */
	constexpr int32 TallyZOrder = 20;
	constexpr int32 ChampionZOrder = 21;

	/**
	 * La música de victoria del podio se pide pasado este tiempo de la fase: para entonces el director de la música de
	 * fin de partida (UTN_MatchMusicSubsystem, Results) ya ha fijado su resultado (1,6 s) y no la pisa con la derrota.
	 */
	constexpr float VictoryDelay = 2.2f;

	/** Segundos que dura la vista previa del recuento (con la cuenta atrás de mentira). */
	constexpr float PreviewTallySeconds = 8.f;

	APlayerController* FindLocalController(UWorld& World)
	{
		for (FConstPlayerControllerIterator It = World.GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Controller = It->Get();
			if (Controller && Controller->IsLocalController()) { return Controller; }
		}
		return nullptr;
	}

	UTN_RaceScreensSubsystem* ScreensOf(UWorld* World)
	{
		UTN_RaceScreensSubsystem* Screens = World ? World->GetSubsystem<UTN_RaceScreensSubsystem>() : nullptr;
		if (!Screens) { UE_LOG(LogTortunabo, Display, TEXT("[Carrera] Aquí no hay pantallas de la carrera (servidor dedicado o sin mundo de juego).")); }
		return Screens;
	}

	int32 IntArg(const TArray<FString>& Args, int32 Index, int32 Default)
	{
		return Args.IsValidIndex(Index) ? FCString::Atoi(*Args[Index]) : Default;
	}

	void RunTally(const TArray<FString>& Args, UWorld* World)
	{
		if (UTN_RaceScreensSubsystem* Screens = ScreensOf(World))
		{
			Screens->StartTallyPreview(IntArg(Args, 0, 0), IntArg(Args, 1, 4), IntArg(Args, 2, 0) != 0);
		}
	}

	void RunPodium(const TArray<FString>& Args, UWorld* World)
	{
		if (UTN_RaceScreensSubsystem* Screens = ScreensOf(World)) { Screens->StartPodiumPreview(IntArg(Args, 0, 3)); }
	}

	void RunPreviewOff(UWorld* World)
	{
		if (UTN_RaceScreensSubsystem* Screens = ScreensOf(World)) { Screens->StopPreview(); }
	}

	FAutoConsoleCommandWithWorldAndArgs CmdTally(TEXT("TN.Race.Tally"),
		TEXT("Vista previa del recuento de conchas: TN.Race.Tally [ganador 0-5, -1 = nadie] [jugadores 1-6] [1 = la concha que corona y luego el podio]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunTally));

	FAutoConsoleCommandWithWorldAndArgs CmdPodium(TEXT("TN.Race.Podium"),
		TEXT("Vista previa de la pantalla del campeón con el podio animado: TN.Race.Podium [jugadores 1-3]. Cualquier botón la cierra."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunPodium));

	FAutoConsoleCommandWithWorld CmdPreviewOff(TEXT("TN.Race.PreviewOff"),
		TEXT("Cierra la vista previa del recuento o del podio."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&RunPreviewOff));
}

// ─────────────────────────────────────────────────────────────────────────────
// Subsistema
// ─────────────────────────────────────────────────────────────────────────────

bool UTN_RaceScreensSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

bool UTN_RaceScreensSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UTN_RaceScreensSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTN_RaceScreensSubsystem, STATGROUP_Tickables);
}

void UTN_RaceScreensSubsystem::Deinitialize()
{
	HideAll();
	Super::Deinitialize();
}

void UTN_RaceScreensSubsystem::Tick(float DeltaTime)
{
	using namespace TNRaceScreensDetail;
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown) { return; }
	APlayerController* PC = FindLocalController(*World);
	if (!PC) { return; }
	if (bPreview)
	{
		TickPreview(DeltaTime, PC);
		return;
	}
	const ATN_BeachRaceGameState* State = World->GetGameState<ATN_BeachRaceGameState>();
	if (!State)
	{
		if (Tally || ChampionScreen) { HideAll(); }
		bHasPhase = false;
		return;
	}
	TickMatch(DeltaTime, PC, *State);
}

void UTN_RaceScreensSubsystem::TickMatch(float DeltaTime, APlayerController* PC, const ATN_BeachRaceGameState& State)
{
	using namespace TNRaceScreensDetail;
	TrackWins(DeltaTime, State);
	if (State.RacePhase == ETNBeachRacePhase::Waiting || State.RacePhase == ETNBeachRacePhase::Racing) { WarmArt(State); }
	const ETNBeachRacePhase Phase = State.RacePhase;
	const int32 Round = State.CurrentRound;
	if (!bHasPhase || Phase != ShownPhase || (Phase == ETNBeachRacePhase::RoundResults && Round != ShownRound))
	{
		bHasPhase = true;
		ShownPhase = Phase;
		ShownRound = Round;
		PhaseClock = 0.f;
		if (Phase == ETNBeachRacePhase::RoundResults)
		{
			ShowTally(PC, BuildTally(State, PC, State.RoundWinner.Get()));
			LandedRound = Round;
		}
		else if (Phase == ETNBeachRacePhase::Champion)
		{
			// Sin recuento de esta ronda (se saltó directamente al campeón): primero la concha que le corona.
			if (LandedRound != Round)
			{
				ShowTally(PC, BuildTally(State, PC, State.Champion ? State.Champion.Get() : State.RoundWinner.Get()));
				LandedRound = Round;
			}
		}
		else
		{
			// Ronda nueva (o partida nueva tras «Volver a jugar»): ningún recuento pendiente.
			HideAll();
			LandedRound = -1;
		}
	}
	PhaseClock += DeltaTime;

	if (Phase == ETNBeachRacePhase::RoundResults && Tally)
	{
		Tally->SetSecondsLeft(State.PhaseSecondsLeft);
		// El ganador puede llegar por red un poco después que la fase.
		const APlayerState* Winner = State.RoundWinner.Get();
		Tally->UpdateWinner(Winner ? TallyPlayerIds.IndexOfByKey(Winner->GetPlayerId()) : INDEX_NONE);
	}
	else if (Phase == ETNBeachRacePhase::Champion)
	{
		if (Tally) { Tally->SetSecondsLeft(0.f); }
		if (!ChampionScreen && (!Tally || Tally->IsSequenceDone() || Tally->IsDismissing()))
		{
			ShowChampion(PC, BuildChampion(State, PC));
		}
		if (ChampionScreen)
		{
			ChampionScreen->SetSecondsLeft(State.PhaseSecondsLeft);
			if (!bVictoryPlaying && PhaseClock >= VictoryDelay) { PlayVictory(true); }
		}
	}
}

void UTN_RaceScreensSubsystem::TrackWins(float DeltaTime, const ATN_BeachRaceGameState& State)
{
	const bool bRoundPhase = State.RacePhase == ETNBeachRacePhase::Waiting || State.RacePhase == ETNBeachRacePhase::Racing;
	if (!bRoundPhase)
	{
		bTrackingRound = false;
		return;
	}
	if (!bTrackingRound)
	{
		bTrackingRound = true;
		WinsAtRoundStart.Reset();
		RacingClock = 0.f;
	}
	if (State.RacePhase == ETNBeachRacePhase::Racing) { RacingClock += DeltaTime; }
	// Mientras se prepara la ronda y en sus primeros segundos se sigue apuntando (por si algo llega tarde); después, solo
	// los jugadores nuevos: la concha de la ronda nunca entra aquí aunque su RoundWins llegue antes que el recuento.
	const bool bSettling = State.RacePhase == ETNBeachRacePhase::Waiting || RacingClock < 4.f;
	for (const TObjectPtr<APlayerState>& Base : State.PlayerArray)
	{
		const ATN_CoopPlayerState* PS = Cast<ATN_CoopPlayerState>(Base);
		if (!PS) { continue; }
		const int32 Id = PS->GetPlayerId();
		if (bSettling || !WinsAtRoundStart.Contains(Id)) { WinsAtRoundStart.Add(Id, PS->RoundWins); }
	}
}

void UTN_RaceScreensSubsystem::WarmArt(const ATN_BeachRaceGameState& State)
{
	using FMakeTexture = UTexture2D* (*)();
	static const FMakeTexture Fixed[] = { &TNRaceArt::TallyBackdrop, &TNRaceArt::ShellSocket, &TNRaceArt::SoftGlow, &TNRaceArt::Sparkle,
		&TNRaceArt::Crown, &TNRaceArt::SkyGradient, &TNRaceArt::SunGlow, &TNRaceArt::Cloud, &TNRaceArt::SidePanel };
	if (WarmStatic < static_cast<int32>(UE_ARRAY_COUNT(Fixed)))
	{
		Fixed[WarmStatic++]();
		return;
	}
	// Caras de cada jugador con su piel: feliz, con ojos de estrella y mareada (en caché: las ya hechas no cuestan).
	const int32 Count = State.PlayerArray.Num() * 3;
	if (Count == 0) { return; }
	WarmCursor = (WarmCursor + 1) % Count;
	if (const ATN_CoopPlayerState* PS = Cast<ATN_CoopPlayerState>(State.PlayerArray[WarmCursor / 3]))
	{
		static const ETNTurtleFace Faces[] = { ETNTurtleFace::Happy, ETNTurtleFace::Win, ETNTurtleFace::Down };
		TNRaceArt::TurtleFaceFor(this, RowOf(*PS, nullptr).Look, Faces[WarmCursor % 3]);
	}
}

FTNRaceTallyRow UTN_RaceScreensSubsystem::RowOf(const ATN_CoopPlayerState& PS, const APlayerController* PC) const
{
	FTNRaceTallyRow Row;
	Row.Name = PS.GetPlayerName();
	Row.Look.HelmetId = PS.EquippedHelmetId;
	Row.Look.ShellId = PS.EquippedShellId;
	Row.Look.SkinId = PS.EquippedSkinId;
	Row.Look.EyesId = PS.EquippedEyesId;
	Row.bLocal = PC && PC->PlayerState.Get() == &PS;
	Row.WinsBefore = PS.RoundWins;
	return Row;
}

FTNRaceTallySetup UTN_RaceScreensSubsystem::BuildTally(const ATN_BeachRaceGameState& State, const APlayerController* PC, const APlayerState* Winner)
{
	FTNRaceTallySetup Setup;
	TallyPlayerIds.Reset();
	// Columnas en el orden de entrada a la partida (siempre el mismo): cada uno se encuentra en su sitio.
	TArray<const ATN_CoopPlayerState*> Players;
	for (const TObjectPtr<APlayerState>& Base : State.PlayerArray)
	{
		const ATN_CoopPlayerState* PS = Cast<ATN_CoopPlayerState>(Base);
		if (PS && (!PS->IsInactive() || PS == Winner)) { Players.Add(PS); }
	}
	Players.Sort([](const ATN_CoopPlayerState& A, const ATN_CoopPlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
	int32 MostWins = 0;
	for (int32 i = 0; i < Players.Num(); ++i)
	{
		const ATN_CoopPlayerState& PS = *Players[i];
		FTNRaceTallyRow Row = RowOf(PS, PC);
		if (&PS == Winner)
		{
			// La concha de la ronda vuela al hueco siguiente a las que tenía al empezar la ronda.
			const int32* AtStart = WinsAtRoundStart.Find(PS.GetPlayerId());
			Row.WinsBefore = AtStart ? *AtStart : FMath::Max(0, PS.RoundWins - 1);
			Setup.WinnerRow = i;
			MostWins = FMath::Max(MostWins, Row.WinsBefore + 1);
		}
		else
		{
			MostWins = FMath::Max(MostWins, Row.WinsBefore);
		}
		Setup.Rows.Add(Row);
		TallyPlayerIds.Add(PS.GetPlayerId());
	}
	Setup.Target = FMath::Max(FMath::Max(1, State.RoundTarget), MostWins);
	Setup.Round = FMath::Max(1, State.CurrentRound);
	return Setup;
}

FTNRaceChampionSetup UTN_RaceScreensSubsystem::BuildChampion(const ATN_BeachRaceGameState& State, const APlayerController* PC) const
{
	FTNRaceChampionSetup Setup;
	Setup.Target = FMath::Max(1, State.RoundTarget);
	for (const TObjectPtr<APlayerState>& Base : State.Podium)
	{
		if (const ATN_CoopPlayerState* PS = Cast<ATN_CoopPlayerState>(Base)) { Setup.Podium.Add(RowOf(*PS, PC)); }
		if (Setup.Podium.Num() == 3) { break; }
	}
	if (Setup.Podium.Num() == 0)
	{
		// Sin podio replicado todavía: el campeón y, detrás, por conchas.
		TArray<const ATN_CoopPlayerState*> Players;
		for (const TObjectPtr<APlayerState>& Base : State.PlayerArray)
		{
			if (const ATN_CoopPlayerState* PS = Cast<ATN_CoopPlayerState>(Base)) { Players.Add(PS); }
		}
		const APlayerState* ChampionState = State.Champion.Get();
		Players.Sort([ChampionState](const ATN_CoopPlayerState& A, const ATN_CoopPlayerState& B)
		{
			if ((&A == ChampionState) != (&B == ChampionState)) { return &A == ChampionState; }
			return A.RoundWins > B.RoundWins;
		});
		for (int32 i = 0; i < Players.Num() && i < 3; ++i) { Setup.Podium.Add(RowOf(*Players[i], PC)); }
	}
	return Setup;
}

void UTN_RaceScreensSubsystem::ShowTally(APlayerController* PC, const FTNRaceTallySetup& Setup)
{
	using namespace TNRaceScreensDetail;
	if (!Tally || Tally->IsDismissing())
	{
		Tally = CreateWidget<UTN_RaceTallyWidget>(PC, UTN_RaceTallyWidget::StaticClass());
		if (!Tally) { return; }
		Tally->AddToViewport(TallyZOrder);
	}
	Tally->Setup(Setup);
	UE_LOG(LogTortunabo, Log, TEXT("[Carrera] Recuento: ronda %d, %d jugadores, ganador en la columna %d%s."), Setup.Round, Setup.Rows.Num(), Setup.WinnerRow,
		Setup.bPreview ? TEXT(" (vista previa)") : TEXT(""));
}

void UTN_RaceScreensSubsystem::ShowChampion(APlayerController* PC, const FTNRaceChampionSetup& Setup)
{
	using namespace TNRaceScreensDetail;
	if (!ChampionScreen)
	{
		ChampionScreen = CreateWidget<UTN_RaceChampionWidget>(PC, UTN_RaceChampionWidget::StaticClass());
		if (!ChampionScreen) { return; }
		ChampionScreen->AddToViewport(ChampionZOrder);
	}
	ChampionScreen->Setup(Setup);
	if (Setup.bPreview)
	{
		TWeakObjectPtr<UTN_RaceScreensSubsystem> WeakThis(this);
		ChampionScreen->OnPreviewClosed = [WeakThis]()
		{
			if (UTN_RaceScreensSubsystem* Screens = WeakThis.Get()) { Screens->StopPreview(); }
		};
	}
	// El recuento se va con un fundido por debajo mientras entra el podio.
	if (Tally)
	{
		Tally->Dismiss();
		Tally = nullptr;
	}
	UE_LOG(LogTortunabo, Log, TEXT("[Carrera] Pantalla del campeón: %s%s."), Setup.Podium.Num() > 0 ? *Setup.Podium[0].Name : TEXT("nadie"),
		Setup.bPreview ? TEXT(" (vista previa)") : TEXT(""));
}

void UTN_RaceScreensSubsystem::HideAll()
{
	if (IsValid(Tally)) { Tally->Dismiss(); }
	Tally = nullptr;
	if (IsValid(ChampionScreen)) { ChampionScreen->RemoveFromParent(); }
	ChampionScreen = nullptr;
	// La música de victoria del podio la apaga el director al cambiar el flujo (ronda nueva o viaje); la de la vista
	// previa, StopPreview.
	bVictoryPlaying = false;
}

void UTN_RaceScreensSubsystem::PlayVictory(bool bOn)
{
	bVictoryPlaying = bOn;
	UWorld* World = GetWorld();
	if (UTN_MatchMusicSubsystem* Music = World ? World->GetSubsystem<UTN_MatchMusicSubsystem>() : nullptr)
	{
		// La misma pista que la de ganar la carrera, para todos: es la fiesta de la campeona.
		Music->DebugPlayTrack(bOn ? ETNMusicTrack::Victory : ETNMusicTrack::None);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Vista previa por consola
// ─────────────────────────────────────────────────────────────────────────────

void UTN_RaceScreensSubsystem::BuildPreviewRows(APlayerController* PC, int32 NumPlayers, TArray<FTNRaceTallyRow>& OutRows) const
{
	OutRows.Reset();
	const int32 Count = FMath::Clamp(NumPlayers, 1, 6);
	static const TCHAR* FakeNames[] = { TEXT("Coral"), TEXT("Bruma"), TEXT("Perla"), TEXT("Marea"), TEXT("Alga") };
	static const int32 FakeWins[] = { 1, 0, 2, 1, 0, 2 };
	const UMP_GameInstance* GI = PC ? Cast<UMP_GameInstance>(UGameplayStatics::GetGameInstance(PC)) : nullptr;
	const TArray<FName> Skins = GI ? GI->GetCosmeticCatalog(ETNCosmeticCategory::Body) : TArray<FName>();
	const TArray<FName> Shells = GI ? GI->GetCosmeticCatalog(ETNCosmeticCategory::Shell) : TArray<FName>();
	const TArray<FName> Helmets = GI ? GI->GetCosmeticCatalog(ETNCosmeticCategory::Helmet) : TArray<FName>();
	const TArray<FName> Eyes = GI ? GI->GetCosmeticCatalog(ETNCosmeticCategory::Eyes) : TArray<FName>();
	auto Pick = [](const TArray<FName>& List, int32 Index) { return List.Num() > 0 ? List[Index % List.Num()] : NAME_None; };
	for (int32 i = 0; i < Count; ++i)
	{
		FTNRaceTallyRow Row;
		Row.WinsBefore = FakeWins[i];
		if (i == 0)
		{
			// Tu tortuga, con tu aspecto de verdad.
			const ATN_CoopPlayerState* Own = PC ? PC->GetPlayerState<ATN_CoopPlayerState>() : nullptr;
			if (Own)
			{
				Row = RowOf(*Own, PC);
				Row.WinsBefore = FakeWins[i];
			}
			else if (GI)
			{
				Row.Look.HelmetId = GI->GetEquippedHelmetId();
				Row.Look.ShellId = GI->GetEquippedShellId();
				Row.Look.SkinId = GI->GetEquippedSkinId();
				Row.Look.EyesId = GI->GetEquippedEyesId();
			}
			if (Row.Name.IsEmpty()) { Row.Name = TEXT("Tú"); }
			Row.bLocal = true;
		}
		else
		{
			Row.Name = FakeNames[(i - 1) % 5];
			Row.Look.SkinId = Pick(Skins, i * 2 + 1);
			Row.Look.ShellId = (i % 2 == 0) ? Pick(Shells, i) : NAME_None;
			Row.Look.HelmetId = Pick(Helmets, i * 3);
			Row.Look.EyesId = (i == 2) ? Pick(Eyes, i) : NAME_None;
		}
		OutRows.Add(Row);
	}
}

void UTN_RaceScreensSubsystem::StartTallyPreview(int32 WinnerRow, int32 NumPlayers, bool bChampionRound)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? TNRaceScreensDetail::FindLocalController(*World) : nullptr;
	if (!PC) { return; }
	StopPreview();
	bPreview = true;
	PreviewClock = 0.f;
	BuildPreviewRows(PC, NumPlayers, PreviewRows);
	FTNRaceTallySetup Setup;
	Setup.Rows = PreviewRows;
	Setup.Target = 3;
	Setup.Round = bChampionRound ? 5 : 2;
	Setup.bPreview = true;
	Setup.WinnerRow = PreviewRows.IsValidIndex(WinnerRow) ? WinnerRow : INDEX_NONE;
	if (bChampionRound && Setup.Rows.IsValidIndex(Setup.WinnerRow)) { Setup.Rows[Setup.WinnerRow].WinsBefore = Setup.Target - 1; }
	PreviewRows = Setup.Rows;
	PreviewWinner = Setup.WinnerRow;
	bPreviewChampionAfter = bChampionRound && Setup.Rows.IsValidIndex(Setup.WinnerRow);
	ShowTally(PC, Setup);
}

void UTN_RaceScreensSubsystem::StartPodiumPreview(int32 NumPlayers)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? TNRaceScreensDetail::FindLocalController(*World) : nullptr;
	if (!PC) { return; }
	StopPreview();
	bPreview = true;
	bPreviewChampionAfter = false;
	PreviewClock = 0.f;
	BuildPreviewRows(PC, FMath::Clamp(NumPlayers, 1, 3), PreviewRows);
	FTNRaceChampionSetup Setup;
	Setup.Podium = PreviewRows;
	Setup.Target = 3;
	Setup.bPreview = true;
	ShowChampion(PC, Setup);
	PlayVictory(true);
}

void UTN_RaceScreensSubsystem::StopPreview()
{
	if (!bPreview) { return; }
	bPreview = false;
	const bool bWasPlaying = bVictoryPlaying;
	HideAll();
	if (bWasPlaying) { PlayVictory(false); }
	bVictoryPlaying = false;
	bHasPhase = false;
}

void UTN_RaceScreensSubsystem::TickPreview(float DeltaTime, APlayerController* PC)
{
	using namespace TNRaceScreensDetail;
	PreviewClock += DeltaTime;
	if (Tally && !ChampionScreen)
	{
		Tally->SetSecondsLeft(FMath::Max(0.f, PreviewTallySeconds - PreviewClock));
	}
	if (bPreviewChampionAfter)
	{
		// Tras la concha que corona, el podio: el ganador primero y el resto por conchas.
		if (!ChampionScreen && Tally && Tally->IsSequenceDone())
		{
			FTNRaceChampionSetup Setup;
			Setup.Target = 3;
			Setup.bPreview = true;
			TArray<FTNRaceTallyRow> Ordered = PreviewRows;
			if (Ordered.IsValidIndex(PreviewWinner))
			{
				Ordered[PreviewWinner].WinsBefore = Setup.Target;
				const FTNRaceTallyRow Winner = Ordered[PreviewWinner];
				Ordered.RemoveAt(PreviewWinner);
				Ordered.StableSort([](const FTNRaceTallyRow& A, const FTNRaceTallyRow& B) { return A.WinsBefore > B.WinsBefore; });
				Ordered.Insert(Winner, 0);
			}
			Ordered.SetNum(FMath::Min(Ordered.Num(), 3));
			Setup.Podium = Ordered;
			ShowChampion(PC, Setup);
			PlayVictory(true);
		}
		return;
	}
	if (!ChampionScreen && PreviewClock >= PreviewTallySeconds) { StopPreview(); }
}
