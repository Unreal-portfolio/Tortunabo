#include "Kart/TN_KartGameMode.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kart/TN_KartAIController.h"
#include "Kart/TN_KartGameState.h"
#include "Kart/TN_KartPlayerController.h"
#include "Kart/TN_KartTrack.h"
#include "Kismet/GameplayStatics.h"
#include "Multiplayer/MP_GameInstance.h"
#include "Rally/TN_RallyAIController.h"
#include "Rally/TN_RallyLogic.h"
#include "World/ProcMap/TN_ProcMapGenerator.h"
#include "World/ProcMap/TN_ProcMapTypes.h"

namespace TNKartMode
{
	TAutoConsoleVariable<int32> CVarKartBots(TEXT("TN.Kart.Bots"), -1,
		TEXT("Karts: bots de la parrilla (-1 = los que falten hasta MinKarts karts; ?Bots= en la URL manda). Vale para la próxima partida."));

	TAutoConsoleVariable<int32> CVarKartSeats(TEXT("TN.Kart.Seats"), 0,
		TEXT("Karts: tortugas por kart (1 o 2; 0 = las de por defecto; ?Seats= en la URL manda). Vale para la próxima partida."));

	const TCHAR* const DefaultSettingsPath = TEXT("/Game/ProcMap/DA_ProcMapSettings.DA_ProcMapSettings");
	const TCHAR* const DefaultLobbyPath = TEXT("/Game/Maps/Lobby/LVL_Lobby");
	/** Cada cuánto se mira si todas tienen el mapa (s). */
	constexpr double ReadyCheckIntervalSeconds = 0.5;

	int32 DifficultyIndex(ETNProcDifficulty Difficulty)
	{
		return FMath::Clamp(static_cast<int32>(Difficulty), 0, 2);
	}
}

ATN_KartGameMode::ATN_KartGameMode()
{
	GameStateClass = ATN_KartGameState::StaticClass();
	PlayerControllerClass = ATN_KartPlayerController::StaticClass();
	AIControllerClass = ATN_KartAIController::StaticClass();
	// Sin variante del manifest: la pista sale del mapa generado (ATN_KartGameState::PrepareTrack).
	DefaultVariant = NAME_None;
	// Del lobby se llega y al lobby se vuelve sin cortar la conexión.
	bUseSeamlessTravel = true;
	MapSettings = TSoftObjectPtr<UTN_ProcMapSettings>(FSoftObjectPath(TNKartMode::DefaultSettingsPath));
}

void ATN_KartGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	// La dificultad del lobby (como en el cooperativo); ?ProcDifficulty= manda para probar sin lobby.
	const UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance());
	if (GI)
	{
		Difficulty = GI->SelectedProcDifficulty;
		ExpectedHumans = FMath::Max(1, GI->PendingTravelPlayerCount);
	}
	const FString DifficultyOption = UGameplayStatics::ParseOption(Options, TEXT("ProcDifficulty"));
	if (DifficultyOption.Equals(TEXT("Easy"), ESearchCase::IgnoreCase)) { Difficulty = ETNProcDifficulty::Easy; }
	else if (DifficultyOption.Equals(TEXT("Normal"), ESearchCase::IgnoreCase)) { Difficulty = ETNProcDifficulty::Normal; }
	else if (DifficultyOption.Equals(TEXT("Hard"), ESearchCase::IgnoreCase)) { Difficulty = ETNProcDifficulty::Hard; }
	if (Difficulty == ETNProcDifficulty::Count)
	{
		Difficulty = ETNProcDifficulty::Normal;
	}
	UrlSeed = UGameplayStatics::GetIntOption(Options, TEXT("ProcSeed"), 0);
	bReturnToLobbyAfterResults = !UGameplayStatics::HasOption(Options, TEXT("Races"));

	// Plazas y bots: lo que no diga la URL lo pone el modo (la carrera del Rally lo lee de las opciones).
	FString KartOptions = Options;
	int32 KartSeats = FMath::Clamp(DefaultSeats, 1, 2);
	if (UGameplayStatics::HasOption(Options, TEXT("Seats")))
	{
		KartSeats = FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("Seats"), KartSeats), 1, 2);
	}
	else
	{
		const int32 Forced = TNKartMode::CVarKartSeats.GetValueOnGameThread();
		KartSeats = Forced == 1 || Forced == 2 ? Forced : KartSeats;
		KartOptions += FString::Printf(TEXT("?Seats=%d"), KartSeats);
	}
	if (!UGameplayStatics::HasOption(Options, TEXT("Bots")))
	{
		const int32 Forced = TNKartMode::CVarKartBots.GetValueOnGameThread();
		const int32 HumanKarts = FMath::DivideAndRoundUp(ExpectedHumans, KartSeats);
		const int32 KartBots = Forced >= 0 ? FMath::Min(Forced, TNRally::MaxGridSlots)
			: FMath::Clamp(MinKarts - HumanKarts, 0, TNRally::MaxGridSlots - HumanKarts);
		KartOptions += FString::Printf(TEXT("?Bots=%d"), KartBots);
	}
	UE_LOG(LogTNRally, Log, TEXT("[Karts] Karts en el mapa del cooperativo: dificultad %s, semilla %s, %d tortuga(s) esperada(s), %d por kart."),
		*UEnum::GetValueAsString(Difficulty), UrlSeed != 0 ? *FString::FromInt(UrlSeed) : TEXT("aleatoria"), ExpectedHumans, KartSeats);
	Super::InitGame(MapName, KartOptions, ErrorMessage);
}

ATN_KartGameState* ATN_KartGameMode::GetKartState() const
{
	return GetGameState<ATN_KartGameState>();
}

ATN_ProcMapGenerator* ATN_KartGameMode::EnsureGenerator()
{
	if (!Generator)
	{
		for (TActorIterator<ATN_ProcMapGenerator> It(GetWorld()); It; ++It)
		{
			Generator = *It;
			break;
		}
	}
	if (!Generator)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Generator = GetWorld()->SpawnActor<ATN_ProcMapGenerator>(ATN_ProcMapGenerator::StaticClass(), FTransform::Identity, Params);
		UE_LOG(LogTNRally, Log, TEXT("[Karts] El nivel no tenía generador: creado %s."), *GetNameSafe(Generator));
	}
	if (Generator)
	{
		Generator->SetSettingsIfMissing(MapSettings.LoadSynchronous());
	}
	return Generator;
}

ATN_ProcMapGenerator* ATN_KartGameMode::GenerateMap()
{
	ATN_ProcMapGenerator* Map = EnsureGenerator();
	if (!Map)
	{
		return nullptr;
	}
	// El mismo mapa del cooperativo con el camino hecho para el kart (el generador lo sabe por el modo, que se replica).
	MapSeed = UrlSeed != 0 ? UrlSeed : (FixedSeed != 0 ? FixedSeed : FMath::RandRange(1, MAX_int32 - 1));
	Map->ServerGenerate(MapSeed, ETNProcGameMode::Karts, Difficulty);
	WaitStartTime = GetWorld()->GetTimeSeconds();
	UE_LOG(LogTNRally, Log, TEXT("[Karts] Mapa %d generado (semilla %d, %s)."), Map->GetBuiltGeneration(), MapSeed,
		*UEnum::GetValueAsString(Difficulty));
	return Map->IsMapReady() ? Map : nullptr;
}

void ATN_KartGameMode::ConfigureBot(ATN_RallyAIController& Pilot)
{
	const int32 Index = TNKartMode::DifficultyIndex(Difficulty);
	// Un poco de variedad entre bots: ±4 km/h alrededor de la velocidad de la dificultad.
	const float Spread = static_cast<float>((NextBotOrdinal++ % 3) - 1) * 4.f;
	Pilot.MaxSpeedKmh = static_cast<float>(BotMaxSpeedKmh[Index]) + Spread;
	Pilot.SpecialFireChance = FMath::Clamp(static_cast<float>(BotSpecialFireChance[Index]), 0.f, 1.f);
}

void ATN_KartGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Nadie se sienta hasta que todas tienen el mapa y el suelo en su máquina: así salen a la vez y ningún kart cae.
	if (!bPlayersReleased && NewPlayer)
	{
		WaitingPlayers.AddUnique(NewPlayer);
		UE_LOG(LogTNRally, Log, TEXT("[Karts] %s espera a que todas tengan el mapa."), *GetNameSafe(NewPlayer));
		return;
	}
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

void ATN_KartGameMode::NotifyClientTrackReady(APlayerController* Player, int32 Generation)
{
	if (!Player)
	{
		return;
	}
	ClientTrackGeneration.Add(Player, Generation);
	UE_LOG(LogTNRally, Log, TEXT("[Karts] %s tiene la pista y el suelo de la generación %d."), *GetNameSafe(Player), Generation);
}

bool ATN_KartGameMode::AreAllPlayersReady() const
{
	const ATN_KartGameState* KartState = GetKartState();
	if (!KartState || !KartState->IsLocalTrackPlayable())
	{
		return false;
	}
	int32 Connected = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Player = It->Get();
		if (!Player)
		{
			continue;
		}
		++Connected;
		// El anfitrión usa la pista del servidor.
		if (Player->IsLocalController())
		{
			continue;
		}
		const int32* Built = ClientTrackGeneration.Find(It->Get());
		if (!Built || *Built != KartState->MapGeneration)
		{
			return false;
		}
	}
	return Connected >= ExpectedHumans;
}

void ATN_KartGameMode::ReleaseWaitingPlayers(const TCHAR* Why)
{
	bPlayersReleased = true;
	TArray<TWeakObjectPtr<APlayerController>> Waiting = MoveTemp(WaitingPlayers);
	WaitingPlayers.Reset();
	UE_LOG(LogTNRally, Log, TEXT("[Karts] %d tortuga(s) a los karts: %s."), Waiting.Num(), Why);
	for (const TWeakObjectPtr<APlayerController>& Player : Waiting)
	{
		if (Player.IsValid())
		{
			Super::HandleStartingNewPlayer_Implementation(Player.Get());
		}
	}
}

void ATN_KartGameMode::Tick(float DeltaSeconds)
{
	const UWorld* World = GetWorld();
	const ATN_KartGameState* KartState = GetKartState();
	if (World && KartState && !bPlayersReleased && HasActorBegunPlay() && World->GetTimeSeconds() >= NextReadyCheckTime)
	{
		NextReadyCheckTime = World->GetTimeSeconds() + TNKartMode::ReadyCheckIntervalSeconds;
		if (AreAllPlayersReady())
		{
			ReleaseWaitingPlayers(TEXT("todas tienen el mapa"));
		}
		else if (WaitStartTime >= 0.0 && World->GetTimeSeconds() - WaitStartTime > PlayersReadyTimeoutSeconds)
		{
			ReleaseWaitingPlayers(TEXT("tope de espera"));
		}
	}
	// Fin de los resultados: al lobby, antes de que el Rally empiece otra carrera en el mismo mapa.
	if (bReturnToLobbyAfterResults && KartState && KartState->Phase == ETNRallyPhase::Results
		&& KartState->GetServerWorldTimeSeconds() >= KartState->PhaseEndServerTime - LobbyTravelLeadSeconds)
	{
		ReturnToLobbyNow();
	}
	if (!bReturning)
	{
		Super::Tick(DeltaSeconds);
	}
}

void ATN_KartGameMode::Logout(AController* Exiting)
{
	APlayerController* Player = Cast<APlayerController>(Exiting);
	WaitingPlayers.Remove(Player);
	ClientTrackGeneration.Remove(Player);
	Super::Logout(Exiting);
}

void ATN_KartGameMode::ReturnToLobbyNow()
{
	UWorld* World = GetWorld();
	if (bReturning || !World || World->IsInSeamlessTravel())
	{
		return;
	}
	bReturning = true;
	// Peones fuera antes del viaje (karts, peones de artillera y espectadores): al lobby no llega nada de los karts.
	TArray<APawn*> Pawns;
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		Pawns.Add(*It);
	}
	for (APawn* Pawn : Pawns)
	{
		if (IsValid(Pawn))
		{
			Pawn->Destroy();
		}
	}
	// Al lobby del que se salió (lo apunta ATN_HQGameMode); «?game=» quita el alias de los karts de la URL.
	const UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance());
	const FString Lobby = GI && !GI->LobbyReturnMapPath.IsEmpty() ? GI->LobbyReturnMapPath : FString(TNKartMode::DefaultLobbyPath);
	const FString TravelURL = Lobby + TEXT("?game=");
	UE_LOG(LogTNRally, Log, TEXT("[Karts] Vuelta al lobby: %s"), *TravelURL);
	World->ServerTravel(TravelURL);
}
