#include "Rally/TN_ProcRallyGameMode.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Multiplayer/MP_GameInstance.h"
#include "Rally/TN_ProcRallyGameState.h"
#include "Rally/TN_RallyAIController.h"
#include "Rally/TN_RallyLogic.h"
#include "Rally/TN_RallyTrack.h"
#include "World/ProcMap/TN_ProcMapGenerator.h"
#include "World/ProcMap/TN_ProcMapTypes.h"

namespace TNProcRallyMode
{
	TAutoConsoleVariable<int32> CVarRallyBots(TEXT("TN.Rally.Bots"), -1,
		TEXT("Rally en el mapa del cooperativo: bots de la parrilla (-1 = los que falten hasta MinBuggies buggies; ?Bots= en la URL manda)."));

	const TCHAR* const DefaultSettingsPath = TEXT("/Game/ProcMap/DA_ProcMapSettings.DA_ProcMapSettings");
	const TCHAR* const DefaultLobbyPath = TEXT("/Game/Maps/Lobby/LVL_Lobby");

	int32 DifficultyIndex(ETNProcDifficulty Difficulty)
	{
		return FMath::Clamp(static_cast<int32>(Difficulty), 0, 2);
	}
}

ATN_ProcRallyGameMode::ATN_ProcRallyGameMode()
{
	GameStateClass = ATN_ProcRallyGameState::StaticClass();
	// Sin variante del manifest: la pista sale del mapa generado.
	DefaultVariant = NAME_None;
	// Del lobby se llega y al lobby se vuelve sin cortar la conexión.
	bUseSeamlessTravel = true;
	MapSettings = TSoftObjectPtr<UTN_ProcMapSettings>(FSoftObjectPath(TNProcRallyMode::DefaultSettingsPath));
}

void ATN_ProcRallyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	// La dificultad del lobby (como en el cooperativo); ?ProcDifficulty= manda para probar sin lobby.
	if (const UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance()))
	{
		Difficulty = GI->SelectedProcDifficulty;
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
	UE_LOG(LogTNRally, Log, TEXT("[ProcRally] Rally en el mapa del cooperativo: dificultad %s, semilla %s."),
		*UEnum::GetValueAsString(Difficulty), UrlSeed != 0 ? *FString::FromInt(UrlSeed) : TEXT("aleatoria"));
}

ATN_ProcRallyGameState* ATN_ProcRallyGameMode::GetProcRallyState() const
{
	return GetGameState<ATN_ProcRallyGameState>();
}

ATN_ProcMapGenerator* ATN_ProcRallyGameMode::EnsureGenerator()
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
		UE_LOG(LogTNRally, Log, TEXT("[ProcRally] El nivel no tenía generador: creado %s."), *GetNameSafe(Generator));
	}
	if (Generator)
	{
		Generator->SetSettingsIfMissing(MapSettings.LoadSynchronous());
	}
	return Generator;
}

ATN_RallyTrack* ATN_ProcRallyGameMode::BuildRaceTrack(ATN_RallyGameState& RallyState)
{
	ATN_ProcRallyGameState* ProcState = Cast<ATN_ProcRallyGameState>(&RallyState);
	ATN_ProcMapGenerator* Map = EnsureGenerator();
	if (!ProcState || !Map)
	{
		UE_LOG(LogTNRally, Error, TEXT("[ProcRally] Sin GameState del Rally en el mapa o sin generador: no hay pista."));
		return nullptr;
	}
	// El mismo mapa del cooperativo con el camino hecho para el buggy (el generador lo sabe por el modo, que se replica).
	const int32 Seed = UrlSeed != 0 ? UrlSeed : (FixedSeed != 0 ? FixedSeed : FMath::RandRange(1, MAX_int32 - 1));
	Map->ServerGenerate(Seed, ETNProcGameMode::Rally, Difficulty);
	ProcState->ProcDifficulty = Difficulty;
	ProcState->MapSeed = Seed;
	ATN_RallyTrack* Built = ProcState->BuildTrackFromMap(*Map);
	GroundWaitStart = GetWorld()->GetTimeSeconds();
	if (Built)
	{
		UE_LOG(LogTNRally, Log, TEXT("[ProcRally] Mapa %d (semilla %d, %s): pista de %.2f km con %d puertas (~%.1f min a 20 m/s)."),
			Map->GetBuiltGeneration(), Seed, *UEnum::GetValueAsString(Difficulty), Built->GetTrackLengthCm() / 100000.0,
			Built->GetGateCount(), Built->GetTrackLengthCm() / 2000.0 / 60.0);
	}
	return Built;
}

bool ATN_ProcRallyGameMode::IsReadyToSeat() const
{
	const ATN_RallyTrack* RaceTrack = GetTrack();
	if (!Generator || !RaceTrack || !RaceTrack->IsBuilt())
	{
		return true;
	}
	// La colisión del terreno se cocina en segundo plano: los buggies no se crean hasta que hay suelo bajo la parrilla.
	if (GroundWaitStart >= 0.0 && GetWorld()->GetTimeSeconds() - GroundWaitStart > GroundReadyTimeoutSeconds)
	{
		return true;
	}
	for (int32 Slot = 0; Slot < TNRally::MaxGridSlots; ++Slot)
	{
		if (!Generator->MapCollisionUnder(RaceTrack->GetGridSlotTransform(Slot).GetLocation()))
		{
			return false;
		}
	}
	// Con la colisión lista, diagnóstico de la línea del piloto IA (solo con LogTNRally en Verbose).
	if (UE_LOG_ACTIVE(LogTNRally, Verbose) && !bLoggedLineProbe)
	{
		bLoggedLineProbe = true;
		UE_LOG(LogTNRally, Verbose, TEXT("[ProcRally] La línea IA choca en %d sitios."), RaceTrack->LogLineObstructions());
	}
	return true;
}

bool ATN_ProcRallyGameMode::AreClientsReady() const
{
	const ATN_ProcRallyGameState* ProcState = GetProcRallyState();
	const int32 Wanted = ProcState ? ProcState->ProcGeneration : 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Player = It->Get();
		// El anfitrión usa la pista del servidor.
		if (!Player || Player->IsLocalController())
		{
			continue;
		}
		const int32* Built = ClientTrackGeneration.Find(It->Get());
		if (!Built || *Built != Wanted)
		{
			return false;
		}
	}
	return true;
}

int32 ATN_ProcRallyGameMode::GetExpectedHumans() const
{
	const UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance());
	return GI ? FMath::Max(0, GI->PendingTravelPlayerCount) : 0;
}

int32 ATN_ProcRallyGameMode::ResolveBotCount() const
{
	if (bBotsFromUrl)
	{
		return Bots;
	}
	const int32 Forced = TNProcRallyMode::CVarRallyBots.GetValueOnGameThread();
	if (Forced >= 0)
	{
		return Forced;
	}
	// Relleno: con las que vienen del lobby (o las que ya están) se cuentan los buggies y se completan hasta MinBuggies.
	int32 Connected = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		Connected += It->IsValid() ? 1 : 0;
	}
	const int32 Humans = FMath::Max(GetExpectedHumans(), Connected);
	const int32 HumanBuggies = FMath::DivideAndRoundUp(Humans, FMath::Max(1, GetSeats()));
	return FMath::Clamp(MinBuggies - HumanBuggies, 0, TNRally::MaxGridSlots - HumanBuggies);
}

void ATN_ProcRallyGameMode::ConfigureBot(ATN_RallyAIController& Pilot, int32 BotIndex)
{
	using namespace TNProcRallyMode;
	const int32 Index = DifficultyIndex(Difficulty);
	// Un poco de variedad entre bots: ±4 km/h alrededor de la velocidad de la dificultad.
	const float Spread = static_cast<float>((BotIndex % 3) - 1) * 4.f;
	Pilot.MaxSpeedKmh = static_cast<float>(BotMaxSpeedKmh[Index]) + Spread;
	Pilot.SpecialFireChance = FMath::Clamp(static_cast<float>(BotSpecialFireChance[Index]), 0.f, 1.f);
}

void ATN_ProcRallyGameMode::NotifyClientTrackReady(APlayerController* Player, int32 Generation)
{
	if (!Player)
	{
		return;
	}
	ClientTrackGeneration.Add(Player, Generation);
	UE_LOG(LogTNRally, Log, TEXT("[ProcRally] %s tiene la pista de la generación %d."), *GetNameSafe(Player), Generation);
}

void ATN_ProcRallyGameMode::Logout(AController* Exiting)
{
	ClientTrackGeneration.Remove(Cast<APlayerController>(Exiting));
	Super::Logout(Exiting);
}

void ATN_ProcRallyGameMode::FinishSession()
{
	// Pruebas de la IA con ?Races=N: se cierra como en LVL_Rally.
	if (RaceLimit > 0 && GetRacesRun() >= RaceLimit)
	{
		Super::FinishSession();
		return;
	}
	ReturnToLobbyNow();
}

void ATN_ProcRallyGameMode::ReturnToLobbyNow()
{
	UWorld* World = GetWorld();
	if (bReturning || !World || World->IsInSeamlessTravel())
	{
		return;
	}
	bReturning = true;
	// Peones fuera antes del viaje (buggies, peones de artillera y espectadores): al lobby no llega nada del Rally.
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
	// Al lobby del que se salió (lo apunta ATN_HQGameMode); «?game=» quita el alias del Rally de la URL.
	const UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance());
	const FString Lobby = GI && !GI->LobbyReturnMapPath.IsEmpty() ? GI->LobbyReturnMapPath : FString(TNProcRallyMode::DefaultLobbyPath);
	const FString TravelURL = Lobby + TEXT("?game=");
	UE_LOG(LogTNRally, Log, TEXT("[ProcRally] Vuelta al lobby: %s"), *TravelURL);
	World->ServerTravel(TravelURL);
}
