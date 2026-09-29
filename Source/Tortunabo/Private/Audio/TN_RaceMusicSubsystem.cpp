#include "Audio/TN_RaceMusicSubsystem.h"
#include "Audio/TN_MusicSynthComponent.h"
#include "Audio/TN_RaceMusicComponent.h"
#include "Core/TN_CoopPlayerState.h"
#include "Core/TN_Log.h"
#include "Game/TN_BeachRaceGameState.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Stats/Stats.h"

// Ayudas de este fichero en un espacio de nombres propio (compilación unity: nada suelto en el ámbito global).
namespace TNRaceMusicLocal
{
	float GVolume = 1.f;
	FAutoConsoleVariableRef CVarVolume(
		TEXT("TN.Race.Music.Volume"),
		GVolume,
		TEXT("Volumen de la música de fondo de la carrera, de 0 a 1,5 (1 por defecto; se suma al deslizador de Música del menú de pausa)."));

	int32 GForce = -1;
	FAutoConsoleVariableRef CVarForce(
		TEXT("TN.Race.Music.Force"),
		GForce,
		TEXT("Prueba de la música de la carrera: -1 = la decide el estado de la partida (por defecto), 0 = callada, 1 = sonando en cualquier mapa."));

	float GTension = -1.f;
	FAutoConsoleVariableRef CVarTension(
		TEXT("TN.Race.Music.Tension"),
		GTension,
		TEXT("Fuerza la capa de tensión de la música de la carrera (0 a 1); -1 = la decide la partida (por defecto)."));

	float GDuck = -1.f;
	FAutoConsoleVariableRef CVarDuck(
		TEXT("TN.Race.Music.Duck"),
		GDuck,
		TEXT("Fuerza cuánto se aparta la música de la carrera (0 a 1: -10 dB y paso bajo); -1 = lo decide la partida (por defecto)."));

	int32 GLayers = 31;
	FAutoConsoleVariableRef CVarLayers(
		TEXT("TN.Race.Music.Layers"),
		GLayers,
		TEXT("Capas de la música de la carrera (suma): 1 ritmo, 2 armonía, 4 melodía, 8 corneta, 16 tensión; 31 = todas (por defecto)."));

	int32 GDebug = 0;
	FAutoConsoleVariableRef CVarDebug(
		TEXT("TN.Race.Music.Debug"),
		GDebug,
		TEXT("1 = escribe en el registro cada cambio de decisión de la música de la carrera y, cada 5 s, cómo va el motor."));

	/** Diez fotos por segundo: de sobra para la música (el estado se replica a 10-30 Hz). */
	constexpr double PollIntervalSeconds = 0.1;
	/** Tiempo en silencio tras el cual se destruye el componente (vuelve a crearse al volver a sonar). */
	constexpr double ReleaseAfterSilentSeconds = 20.0;

	APlayerController* FindLocalController(UWorld& InWorld)
	{
		for (FConstPlayerControllerIterator It = InWorld.GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Controller = It->Get();
			if (Controller && Controller->IsLocalController())
			{
				return Controller;
			}
		}
		return nullptr;
	}

	TNRaceMusic::EPhase ToPhase(ETNBeachRacePhase InPhase)
	{
		switch (InPhase)
		{
		case ETNBeachRacePhase::Racing: return TNRaceMusic::EPhase::Racing;
		case ETNBeachRacePhase::RoundResults: return TNRaceMusic::EPhase::RoundResults;
		case ETNBeachRacePhase::Champion: return TNRaceMusic::EPhase::Champion;
		case ETNBeachRacePhase::SprintIntro: return TNRaceMusic::EPhase::SprintIntro;
		default: return TNRaceMusic::EPhase::Waiting;
		}
	}

	TNRaceMusic::EFinish ToFinish(ETNBeachFinishCountdown InFinish)
	{
		switch (InFinish)
		{
		case ETNBeachFinishCountdown::Counting: return TNRaceMusic::EFinish::Counting;
		case ETNBeachFinishCountdown::TimeUp: return TNRaceMusic::EFinish::TimeUp;
		case ETNBeachFinishCountdown::AllIn: return TNRaceMusic::EFinish::AllIn;
		default: return TNRaceMusic::EFinish::None;
		}
	}

	void HandlePlayCommand(const TArray<FString>& InArgs, UWorld* /*InWorld*/)
	{
		GForce = 1;
		if (InArgs.Num() >= 1) { GTension = FMath::Clamp(FCString::Atof(*InArgs[0]), 0.f, 1.f); }
		if (InArgs.Num() >= 2) { GDuck = FMath::Clamp(FCString::Atof(*InArgs[1]), 0.f, 1.f); }
		UE_LOG(LogTortunabo, Display, TEXT("[RaceMusic] Prueba: suena en cualquier mapa (tensión %.2f, duck %.2f; -1 = automático). TN.Race.Music.Auto para volver al modo normal."), GTension, GDuck);
	}

	void HandleStopCommand(const TArray<FString>& /*InArgs*/, UWorld* /*InWorld*/)
	{
		GForce = 0;
		UE_LOG(LogTortunabo, Display, TEXT("[RaceMusic] Prueba: callada. TN.Race.Music.Auto para volver al modo normal."));
	}

	void HandleAutoCommand(const TArray<FString>& /*InArgs*/, UWorld* /*InWorld*/)
	{
		GForce = -1;
		GTension = -1.f;
		GDuck = -1.f;
		GLayers = 31;
		UE_LOG(LogTortunabo, Display, TEXT("[RaceMusic] Modo normal: la decide el estado de la partida."));
	}

	void HandleRestartCommand(const TArray<FString>& /*InArgs*/, UWorld* InWorld)
	{
		if (UTN_RaceMusicSubsystem* RaceMusic = InWorld ? InWorld->GetSubsystem<UTN_RaceMusicSubsystem>() : nullptr)
		{
			RaceMusic->DebugRestart();
		}
	}

	void HandleStatusCommand(const TArray<FString>& /*InArgs*/, UWorld* InWorld)
	{
		UTN_RaceMusicSubsystem* RaceMusic = InWorld ? InWorld->GetSubsystem<UTN_RaceMusicSubsystem>() : nullptr;
		if (!RaceMusic)
		{
			UE_LOG(LogTortunabo, Display, TEXT("[RaceMusic] Aquí no hay música de carrera (servidor dedicado, sin audio o sin mundo de juego)."));
			return;
		}
		RaceMusic->LogStatus();
	}

	FAutoConsoleCommandWithWorldAndArgs PlayCommand(
		TEXT("TN.Race.Music.Play"),
		TEXT("Hace sonar la música de la carrera en cualquier mapa: TN.Race.Music.Play [tensión 0..1] [duck 0..1]. TN.Race.Music.Auto vuelve al modo normal."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandlePlayCommand));

	FAutoConsoleCommandWithWorldAndArgs StopCommand(
		TEXT("TN.Race.Music.Stop"),
		TEXT("Calla la música de la carrera (fundido). TN.Race.Music.Auto vuelve al modo normal."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleStopCommand));

	FAutoConsoleCommandWithWorldAndArgs AutoCommand(
		TEXT("TN.Race.Music.Auto"),
		TEXT("Vuelve al modo normal: la música de la carrera la decide el estado de la partida (quita las pruebas de Force, Tension, Duck y Layers)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleAutoCommand));

	FAutoConsoleCommandWithWorldAndArgs RestartCommand(
		TEXT("TN.Race.Music.Restart"),
		TEXT("Vuelve a empezar la música de la carrera desde la introducción."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleRestartCommand));

	FAutoConsoleCommandWithWorldAndArgs StatusCommand(
		TEXT("TN.Race.Music.Status"),
		TEXT("Escribe en el registro qué decide el director de la música de la carrera y cómo va el motor (compás, vuelta, tensión, duck)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleStatusCommand));
}

// ─────────────────────────────────────────────────────────────────────────────
// UTN_RaceMusicSubsystem
// ─────────────────────────────────────────────────────────────────────────────

bool UTN_RaceMusicSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Solo donde alguien escucha: ni servidor dedicado ni ejecuciones sin audio.
	return !IsRunningDedicatedServer() && FApp::CanEverRenderAudio() && Super::ShouldCreateSubsystem(Outer);
}

bool UTN_RaceMusicSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UTN_RaceMusicSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &UTN_RaceMusicSubsystem::HandleWorldBeginTearDown);
}

void UTN_RaceMusicSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldBeginTearDown.Remove(TearDownHandle);
	TearDownHandle.Reset();
	ReleaseMusic();
	Director.Reset();
	Super::Deinitialize();
}

TStatId UTN_RaceMusicSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTN_RaceMusicSubsystem, STATGROUP_Tickables);
}

void UTN_RaceMusicSubsystem::HandleWorldBeginTearDown(UWorld* InWorld)
{
	if (InWorld && InWorld == GetWorld())
	{
		// Vuelta al lobby, salida al menú u otro mapa: el PlayerController viaja al mundo nuevo, pero sin esta música.
		ReleaseMusic();
		Director.Reset();
	}
}

void UTN_RaceMusicSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown)
	{
		return;
	}
	const double NowSeconds = World->GetRealTimeSeconds();
	if (NowSeconds < NextPollSeconds)
	{
		return;
	}
	NextPollSeconds = NowSeconds + TNRaceMusicLocal::PollIntervalSeconds;
	Poll(*World, NowSeconds);
}

bool UTN_RaceMusicSubsystem::IsMatchMusicPlaying(const APlayerController* InLocalController) const
{
	if (!InLocalController)
	{
		return false;
	}
	// La música de fin de partida es un UTN_MusicSynthComponent de la clase base en este mismo PlayerController; la de
	// esta carrera es una clase hija y no cuenta.
	TInlineComponentArray<UTN_MusicSynthComponent*> MusicComponents(InLocalController);
	for (const UTN_MusicSynthComponent* Candidate : MusicComponents)
	{
		if (!Candidate || Candidate->GetClass() != UTN_MusicSynthComponent::StaticClass())
		{
			continue;
		}
		switch (Candidate->GetRequestedTrack())
		{
		case ETNMusicTrack::Victory:
		case ETNMusicTrack::Defeat:
		case ETNMusicTrack::Eliminated:
			return true;
		default:
			break;
		}
	}
	return false;
}

void UTN_RaceMusicSubsystem::Poll(UWorld& InWorld, double InNowSeconds)
{
	APlayerController* LocalController = TNRaceMusicLocal::FindLocalController(InWorld);
	const ATN_BeachRaceGameState* RaceState = InWorld.GetGameState<ATN_BeachRaceGameState>();
	const int32 Force = TNRaceMusicLocal::GForce;
	// Fuera de la carrera y sin nada que probar, no hay nada que hacer (ni siquiera crear el componente).
	if (!LocalController || (!RaceState && Force < 0 && !Music.IsValid()))
	{
		return;
	}

	TNRaceMusic::FDirectorDecision Decision;
	if (Force >= 0)
	{
		Decision.bPlay = Force > 0;
		Decision.FadeSeconds = 1.5f;
		Decision.Reason = "prueba forzada";
	}
	else
	{
		TNRaceMusic::FDirectorSnapshot Snap;
		Snap.NowSeconds = InNowSeconds;
		if (RaceState)
		{
			Snap.bRaceWorld = true;
			Snap.Phase = TNRaceMusicLocal::ToPhase(RaceState->RacePhase);
			Snap.bSprintFinal = RaceState->bSprintFinal;
			Snap.PhaseSecondsLeft = RaceState->PhaseSecondsLeft;
			Snap.Finish = TNRaceMusicLocal::ToFinish(RaceState->FinishCountdown);
			Snap.FinishSecondsLeft = RaceState->GetFinishCountdownLeft();
			Snap.FinishSecondsTotal = RaceState->FinishCountdownSeconds;
			Snap.RoundSecondsLeft = RaceState->GetRoundTimeLeft();
		}
		if (const ATN_CoopPlayerState* LocalState = LocalController->GetPlayerState<ATN_CoopPlayerState>())
		{
			// Como UTN_MatchMusicSubsystem: llegar a la meta es terminar sin estar eliminado.
			Snap.bLocalFinished = LocalState->bHasFinishedRun && !LocalState->bIsEliminated;
		}
		Snap.bOtherMusicPlaying = IsMatchMusicPlaying(LocalController);
		Decision = Director.Update(Snap);
	}
	if (TNRaceMusicLocal::GTension >= 0.f) { Decision.Tension = FMath::Clamp(TNRaceMusicLocal::GTension, 0.f, 1.f); }
	if (TNRaceMusicLocal::GDuck >= 0.f) { Decision.Duck = FMath::Clamp(TNRaceMusicLocal::GDuck, 0.f, 1.f); }

	Apply(LocalController, Decision, InNowSeconds);
}

void UTN_RaceMusicSubsystem::Apply(APlayerController* InLocalController, const TNRaceMusic::FDirectorDecision& InDecision, double InNowSeconds)
{
	const bool bDebug = TNRaceMusicLocal::GDebug != 0;
	if (bDebug && (InDecision.bPlay != LastDecision.bPlay || FCStringAnsi::Strcmp(InDecision.Reason, LastDecision.Reason) != 0))
	{
		UE_LOG(LogTortunabo, Display, TEXT("[RaceMusic] %s (%s): fundido %.1f s, tensión %.2f, duck %.2f"),
			InDecision.bPlay ? TEXT("suena") : TEXT("calla"), UTF8_TO_TCHAR(InDecision.Reason), InDecision.FadeSeconds, InDecision.Tension, InDecision.Duck);
	}
	LastDecision = InDecision;

	UTN_RaceMusicComponent* Comp = InDecision.bPlay ? EnsureMusic(InLocalController) : Music.Get();
	if (!Comp)
	{
		return;
	}
	if (InDecision.bPlay != bAppliedPlay)
	{
		bAppliedPlay = InDecision.bPlay;
		Comp->SetRacePlaying(InDecision.bPlay, InDecision.FadeSeconds);
		if (!InDecision.bPlay) { SilentSinceSeconds = InNowSeconds; }
	}
	Comp->SetRaceTension(InDecision.Tension);
	Comp->SetRaceDuck(InDecision.Duck);
	Comp->SetRaceVolume(FMath::Clamp(TNRaceMusicLocal::GVolume, 0.f, 1.5f));
	Comp->SetRaceLayerMask(TNRaceMusicLocal::GLayers);

	if (bDebug && InNowSeconds >= NextDebugLogSeconds)
	{
		NextDebugLogSeconds = InNowSeconds + 5.0;
		const FTNRaceMusicDebugInfo Info = Comp->GetRaceDebugInfo();
		UE_LOG(LogTortunabo, Display, TEXT("[RaceMusic] motor %s, compás %d, vuelta %d, tensión %.2f, duck %.2f"),
			Info.bRunning ? TEXT("en marcha") : TEXT("parado"), Info.Bar + 1, Info.Pass + 1, Info.Tension, Info.Duck);
	}

	// En silencio y con el motor parado desde hace rato: fuera el componente (vuelve a crearse al volver a sonar).
	if (!bAppliedPlay && InNowSeconds - SilentSinceSeconds > TNRaceMusicLocal::ReleaseAfterSilentSeconds && !Comp->GetRaceDebugInfo().bRunning)
	{
		ReleaseMusic();
	}
}

UTN_RaceMusicComponent* UTN_RaceMusicSubsystem::EnsureMusic(APlayerController* InLocalController)
{
	UTN_RaceMusicComponent* Existing = Music.Get();
	if (Existing && !Existing->IsBeingDestroyed() && Existing->GetOwner() == InLocalController)
	{
		return Existing;
	}
	ReleaseMusic();
	if (!InLocalController)
	{
		return nullptr;
	}
	UTN_RaceMusicComponent* Created = UTN_RaceMusicComponent::AttachRaceMusic(InLocalController);
	if (Created)
	{
		Created->SetRaceVolume(FMath::Clamp(TNRaceMusicLocal::GVolume, 0.f, 1.5f));
		Music = Created;
		bAppliedPlay = false;
	}
	return Created;
}

void UTN_RaceMusicSubsystem::ReleaseMusic()
{
	if (UTN_RaceMusicComponent* Existing = Music.Get())
	{
		if (IsValid(Existing) && !Existing->IsBeingDestroyed())
		{
			Existing->DestroyComponent();
		}
	}
	Music.Reset();
	bAppliedPlay = false;
}

void UTN_RaceMusicSubsystem::DebugRestart()
{
	if (UTN_RaceMusicComponent* Existing = Music.Get())
	{
		Existing->RestartRacePiece();
		UE_LOG(LogTortunabo, Display, TEXT("[RaceMusic] Reinicio: vuelve a la introducción."));
	}
	else
	{
		UE_LOG(LogTortunabo, Display, TEXT("[RaceMusic] Reinicio: no hay música sonando (TN.Race.Music.Play para empezar)."));
	}
}

void UTN_RaceMusicSubsystem::LogStatus() const
{
	const UWorld* World = GetWorld();
	const ATN_BeachRaceGameState* RaceState = World ? World->GetGameState<ATN_BeachRaceGameState>() : nullptr;
	UE_LOG(LogTortunabo, Display, TEXT("[RaceMusic] Mundo de carrera: %s. Forzado: %d (tensión %.2f, duck %.2f, capas %d, volumen %.2f)."),
		RaceState ? TEXT("sí") : TEXT("no"), TNRaceMusicLocal::GForce, TNRaceMusicLocal::GTension, TNRaceMusicLocal::GDuck,
		TNRaceMusicLocal::GLayers, TNRaceMusicLocal::GVolume);
	UE_LOG(LogTortunabo, Display, TEXT("[RaceMusic] Última decisión: %s (%s), tensión %.2f, duck %.2f, fundido %.1f s."),
		LastDecision.bPlay ? TEXT("suena") : TEXT("calla"), UTF8_TO_TCHAR(LastDecision.Reason), LastDecision.Tension, LastDecision.Duck, LastDecision.FadeSeconds);
	if (const UTN_RaceMusicComponent* Comp = Music.Get())
	{
		const FTNRaceMusicDebugInfo Info = Comp->GetRaceDebugInfo();
		UE_LOG(LogTortunabo, Display, TEXT("[RaceMusic] Motor %s: compás %d de 32, vuelta %d de 2, tensión suavizada %.2f, duck suavizado %.2f."),
			Info.bRunning ? TEXT("en marcha") : TEXT("parado"), Info.Bar + 1, Info.Pass + 1, Info.Tension, Info.Duck);
	}
	else
	{
		UE_LOG(LogTortunabo, Display, TEXT("[RaceMusic] Sin componente de música (se crea al hacer falta sonar)."));
	}
}
