#include "Multiplayer/TN_TravelFailureSubsystem.h"
#include "Multiplayer/MP_GameInstance.h"
#include "Multiplayer/TN_TravelFailureDecisions.h"
#include "Core/TN_CoopGameState.h"
#include "Core/TN_Log.h"
#include "Core/TN_MatchFlowTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectGlobals.h"

namespace TNTravelFailureDetail
{
	/** El aviso del menú tras un viaje fallido (también sirve para reconocerlo al retirarlo). */
	FText FailedNotice()
	{
		return NSLOCTEXT("TNRooms", "TravelFailed", "No se ha podido cargar la partida y has vuelto al menú.");
	}

	/**
	 * true si World es el lobby al que se volvería y sigue en pie. Deja de estarlo en cuanto lanza la partida (cuenta atrás o
	 * pausa antes de viajar): ATN_HQGameMode::BeginMatchTravel destruye las tortugas antes de pedir el viaje, así que tras un
	 * lanzamiento fallido hay que recargarlo.
	 */
	bool IsLobbyIntact(const UWorld* World, const FString& LobbyReturnMapPath)
	{
		if (!World || !TNTravel::IsLobbyMap(UWorld::RemovePIEPrefix(World->GetMapName()), LobbyReturnMapPath))
		{
			return false;
		}
		const ATN_CoopGameState* State = World->GetGameState<ATN_CoopGameState>();
		if (!State)
		{
			return true;
		}
		const bool bLaunching = State->CountdownValue > 0
			|| State->MatchFlowState == ETNMatchFlowState::Countdown
			|| State->MatchFlowState == ETNMatchFlowState::Cinematic;
		return !bLaunching;
	}

	/**
	 * El anfitrión sigue en su sesión (se queda en el lobby o lo recarga). UEngine::HandleTravelFailure, enlazado antes que este
	 * subsistema, ya ha llamado a UEngine::HandleDisconnect: deja pedido un viaje a «?closed» (la entrada por defecto, el menú)
	 * para el próximo tick del motor, cuyo LoadMap cancelaría hasta un viaje sin cortes en marcha, y le quita ?Listen a la última
	 * URL. Se anula lo uno y se devuelve lo otro.
	 */
	void KeepHostInSession(const UWorld* World)
	{
		FWorldContext* Context = (GEngine && World) ? GEngine->GetWorldContextFromWorld(World) : nullptr;
		if (!Context)
		{
			return;
		}
		Context->TravelURL.Reset();
		if (World->GetNetMode() == NM_ListenServer && !Context->LastURL.HasOption(TEXT("Listen")))
		{
			Context->LastURL.AddOption(TEXT("Listen"));
		}
	}

	/**
	 * Prueba: TN.Travel.Fail [mapa] pide un ServerTravel a un mapa que no existe, que es lo que pasa con un mapa sin cocinar
	 * (en un juego con viaje sin cortes, el fallo llega dentro de ProcessServerTravel; en PIE, con net.AllowPIESeamlessTravel 1).
	 */
	void HandleTestCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogTortunabo, Display, TEXT("[MP] TN.Travel.Fail: hace falta un mundo de anfitrión o servidor."));
			return;
		}
		// Un mapa que no existe a propósito, montado en dos trozos para que Tortunabo.Cook.StringPathsAreCooked no lo tome
		// por una ruta que haya que cocinar.
		const FString Map = Args.Num() > 0 ? Args[0] : FString(TEXT("/Game/Maps/")) + TEXT("TN_MapaQueNoExiste");
		UE_LOG(LogTortunabo, Display, TEXT("[MP] TN.Travel.Fail: ServerTravel a «%s» (no existe)."), *Map);
		World->ServerTravel(Map);
	}

	FAutoConsoleCommandWithWorldAndArgs TestCommand(
		TEXT("TN.Travel.Fail"),
		TEXT("Pide un ServerTravel a un mapa que no existe para probar el viaje fallido: TN.Travel.Fail [/Game/Ruta/Mapa]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleTestCommand));
}

bool UTN_TravelFailureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Outer && Outer->IsA<UMP_GameInstance>() && Super::ShouldCreateSubsystem(Outer);
}

void UTN_TravelFailureSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (GEngine)
	{
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UTN_TravelFailureSubsystem::HandleTravelFailure);
	}
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UTN_TravelFailureSubsystem::HandlePostLoadMap);
}

void UTN_TravelFailureSubsystem::Deinitialize()
{
	CancelLobbyTravel();
	if (GEngine)
	{
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	Super::Deinitialize();
}

UMP_GameInstance* UTN_TravelFailureSubsystem::GetTNGameInstance() const
{
	return Cast<UMP_GameInstance>(GetGameInstance());
}

void UTN_TravelFailureSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	UMP_GameInstance* GI = GetTNGameInstance();
	if (!GI)
	{
		return;
	}
	// OnTravelFailure es de todo el motor: en PIE con varias ventanas, el fallo de otra no es asunto de esta.
	if (World && World->GetGameInstance() && World->GetGameInstance() != GI)
	{
		return;
	}

	// El último fallo manda: un viaje al lobby que esperaba su tick por un fallo anterior ya no toca.
	CancelLobbyTravel();

	const ENetMode NetMode = World ? World->GetNetMode() : NM_Standalone;
	const bool bInMenu = GI->IsInMenuWorld(World);
	const bool bLobbyIntact = TNTravelFailureDetail::IsLobbyIntact(World, GI->LobbyReturnMapPath);
	const TNTravel::ETravelFailureAction Action = TNTravel::DecideTravelFailure(NetMode, bInMenu, bLobbyIntact, FailureCount);
	++FailureCount;
	UE_LOG(LogTortunabo, Error, TEXT("[MP] Fallo de viaje %s: %s → %s (fallo %d seguido)."),
		ETravelFailure::ToString(FailureType), *ErrorString, TNTravel::ActionName(Action), FailureCount);

	// Nada de «Reconectando...»: el corte que venga ahora no es un ServerTravel normal.
	GI->ClearPendingTravel();
	// Quien pidió el viaje dejó su pantalla de carga puesta; aquí se quita, y solo se vuelve a enseñar si el viaje al lobby arranca.
	GI->HideLoadingScreen();

	if (Action == TNTravel::ETravelFailureAction::StayInMenu)
	{
		GI->OnRoomNotice.Broadcast(TNTravelFailureDetail::FailedNotice(), true);
		// Tras un viaje duro fallido el anfitrión acaba aquí con su sesión de Steam viva y sin invitados (ver arriba).
		GI->DestroyCurrentSession();
		return;
	}

	const bool bHostStays = Action == TNTravel::ETravelFailureAction::StayInLobby || Action == TNTravel::ETravelFailureAction::ReturnHostToLobby;
	if (bHostStays)
	{
		TNTravelFailureDetail::KeepHostInSession(World);
	}
	if (Action == TNTravel::ETravelFailureAction::StayInLobby)
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[MP] El lobby sigue en pie: no hay a dónde volver."));
		return;
	}

	// Por si se acaba en el menú (el motor también vuelve a la entrada por defecto si nadie lo impide): allí sale el aviso.
	FTNMenuNotice Notice;
	Notice.Text = TNTravelFailureDetail::FailedNotice();
	Notice.bError = true;
	Notice.bOpenJoin = false;
	GI->SetMenuNotice(Notice);
	bNoticePending = true;

	if (Action == TNTravel::ETravelFailureAction::ReturnHostToLobby && World)
	{
		// En un tick, no ahora: dentro de ProcessServerTravel World->NextURL aún no está vacío y ServerTravel no haría nada.
		LobbyTravelWorld = World;
		LobbyTravelTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UTN_TravelFailureSubsystem::TickLobbyTravel));
		return;
	}

	SendToMenu(*GI, World);
}

bool UTN_TravelFailureSubsystem::TickLobbyTravel(float /*DeltaTime*/)
{
	LobbyTravelTicker.Reset();
	UWorld* World = LobbyTravelWorld.Get();
	LobbyTravelWorld.Reset();

	UMP_GameInstance* GI = GetTNGameInstance();
	if (!GI || !World || World->GetGameInstance() != GI)
	{
		// El mundo ya no está: otro mapa ha ocupado su lugar y el viaje del fallo ya no tiene sentido.
		return false;
	}
	if (World->IsInSeamlessTravel() || !World->NextURL.IsEmpty())
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[MP] Ya hay otro viaje en marcha: no se pide el del lobby."));
		return false;
	}

	const FString LobbyURL = TNTravel::LobbyTravelURL(GI->LobbyReturnMapPath);
	UE_LOG(LogTortunabo, Warning, TEXT("[MP] El anfitrión vuelve al lobby con todos: %s"), *LobbyURL);
	const int32 FailuresBefore = FailureCount;
	const bool bAccepted = World->ServerTravel(LobbyURL);
	if (FailureCount != FailuresBefore)
	{
		// El propio viaje al lobby ha fallado (un lobby sin cocinar): HandleTravelFailure ya ha actuado, y al ser el segundo
		// fallo seguido ha mandado al menú.
		return false;
	}
	if (!TNTravel::DidTravelStart(bAccepted, World->IsInSeamlessTravel(), !World->NextURL.IsEmpty()))
	{
		UE_LOG(LogTortunabo, Error, TEXT("[MP] El viaje al lobby %s no ha arrancado: se oculta la pantalla de carga y se vuelve al menú."), *LobbyURL);
		GI->HideLoadingScreen();
		SendToMenu(*GI, World);
		return false;
	}

	// Ya viaja: ahora sí, la pantalla de carga con el motivo.
	GI->ShowLoadingScreen(NSLOCTEXT("TNRooms", "TravelFailedLobby", "No se ha podido cargar la partida: volvéis al lobby.").ToString());
	return false;
}

void UTN_TravelFailureSubsystem::CancelLobbyTravel()
{
	FTSTicker::RemoveTicker(LobbyTravelTicker);
	LobbyTravelTicker.Reset();
	LobbyTravelWorld.Reset();
}

void UTN_TravelFailureSubsystem::SendToMenu(UMP_GameInstance& GI, UWorld* World)
{
	GI.DestroyCurrentSession();
	if (APlayerController* PC = GI.GetFirstLocalPlayerController(World))
	{
		PC->ClientTravel(GI.GetMenuMapPath(), TRAVEL_Absolute);
	}
}

void UTN_TravelFailureSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	UMP_GameInstance* GI = GetTNGameInstance();
	if (!GI || !LoadedWorld || LoadedWorld->GetGameInstance() != GI)
	{
		return;
	}
	FailureCount = 0;
	if (!bNoticePending)
	{
		return;
	}
	bNoticePending = false;
	if (GI->IsInMenuWorld(LoadedWorld))
	{
		return; // El menú lo enseña (MP_MainMenuWidget lo consume al abrirse).
	}
	// Se ha llegado a otro mapa (el lobby): el aviso ya no toca. Si es otro aviso, se deja donde estaba.
	const FTNMenuNotice Pending = GI->ConsumeMenuNotice();
	if (!Pending.Text.EqualTo(TNTravelFailureDetail::FailedNotice()))
	{
		GI->SetMenuNotice(Pending);
	}
}
