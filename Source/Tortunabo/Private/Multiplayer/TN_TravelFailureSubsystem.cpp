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
#include "TimerManager.h"
#include "Misc/CoreDelegates.h"
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
	 * El anfitrión sigue en su sesión (se queda en el lobby o lo recarga). Si el fallo llegó por UEngine::OnTravelFailure,
	 * UEngine::HandleTravelFailure lo atiende DESPUÉS que este subsistema (Broadcast recorre los delegados del último al primero)
	 * y, con UEngine::HandleDisconnect, deja pedido un viaje a «?closed» (la entrada por defecto, el menú) para el próximo tick
	 * del motor, cuyo LoadMap cancelaría hasta un viaje sin cortes en marcha, y le quita ?Listen a la última URL. Por eso esto se
	 * llama al empezar el fotograma siguiente al fallo (HandleBeginFrameAfterFailure): se anula ese viaje (solo ese) y se
	 * devuelve el ?Listen.
	 */
	void KeepHostInSession(const UWorld* World)
	{
		FWorldContext* Context = (GEngine && World) ? GEngine->GetWorldContextFromWorld(World) : nullptr;
		if (!Context)
		{
			return;
		}
		if (TNTravel::IsEngineDisconnectTravel(Context->TravelURL))
		{
			UE_LOG(LogTortunabo, Display, TEXT("[MP] Se anula el viaje al menú que pidió el motor tras el fallo."));
			Context->TravelURL.Reset();
		}
		if (World->GetNetMode() == NM_ListenServer && !Context->LastURL.HasOption(TEXT("Listen")))
		{
			Context->LastURL.AddOption(TEXT("Listen"));
		}
	}

	/** Argumento de TN.Travel.Fail que simula el fallo por el camino del motor en vez de pedir el viaje. */
	const TCHAR* const EngineFailureArg = TEXT("motor");

	/** Provoca el fallo de prueba: Target es «motor» o el mapa (vacío: uno que no existe). */
	void RunTestFailure(UWorld* World, const FString& Target)
	{
		if (!World || !GEngine)
		{
			return;
		}
		if (Target.Equals(EngineFailureArg, ESearchCase::IgnoreCase))
		{
			UE_LOG(LogTortunabo, Display, TEXT("[MP] TN.Travel.Fail: fallo simulado por UEngine::OnTravelFailure."));
			GEngine->BroadcastTravelFailure(World, ETravelFailure::ServerTravelFailure, TEXT("TN.Travel.Fail motor"));
			return;
		}
		// Un mapa que no existe a propósito, montado en dos trozos para que Tortunabo.Cook.StringPathsAreCooked no lo tome
		// por una ruta que haya que cocinar.
		const FString Map = Target.IsEmpty() ? FString(TEXT("/Game/Maps/")) + TEXT("TN_MapaQueNoExiste") : Target;
		UE_LOG(LogTortunabo, Display, TEXT("[MP] TN.Travel.Fail: ServerTravel a «%s» (no existe)."), *Map);
		World->ServerTravel(Map);
	}

	/**
	 * Prueba: TN.Travel.Fail [mapa | motor] [segundos]. Con un mapa (por defecto uno que no existe) pide un ServerTravel a él, que
	 * es lo que pasa con un mapa sin cocinar (lo para CanServerTravel en HQ y Run). Con «motor» simula un fallo que llega por
	 * UEngine::OnTravelFailure (un mapa que existe pero no carga), con la desconexión que pide el motor detrás. Con segundos, lo
	 * hace pasado ese tiempo (para dar tiempo a que entren invitados en una prueba sin ventana con -ExecCmds).
	 */
	void HandleTestCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogTortunabo, Display, TEXT("[MP] TN.Travel.Fail: hace falta un mundo de anfitrión o servidor."));
			return;
		}
		TArray<FString> Rest = Args;
		const float DelaySeconds = (Rest.Num() > 0 && Rest.Last().IsNumeric()) ? FCString::Atof(*Rest.Pop()) : 0.f;
		const FString Target = Rest.Num() > 0 ? Rest[0] : FString();
		if (DelaySeconds <= 0.f)
		{
			RunTestFailure(World, Target);
			return;
		}
		UE_LOG(LogTortunabo, Display, TEXT("[MP] TN.Travel.Fail: dentro de %.1f s."), DelaySeconds);
		FTimerHandle Handle;
		const TWeakObjectPtr<UWorld> WeakWorld(World);
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakWorld, Target]()
		{
			RunTestFailure(WeakWorld.Get(), Target);
		}), DelaySeconds, false);
	}

	FAutoConsoleCommandWithWorldAndArgs TestCommand(
		TEXT("TN.Travel.Fail"),
		TEXT("Prueba del viaje fallido: TN.Travel.Fail [/Game/Ruta/Mapa | motor] [segundos]. Sin argumentos, ServerTravel a un mapa que no existe; motor simula un fallo de UEngine::OnTravelFailure."),
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
	CancelAfterFailure();
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

bool UTN_TravelFailureSubsystem::DoesTravelMapExist(FString MapPackage)
{
	// Como el motor al viajar: nombres largos o cortos, sin el prefijo de PIE, y los paquetes ya cargados (un mapa sin guardar en PIE).
	return !MapPackage.IsEmpty() && GEngine && GEngine->MakeSureMapNameIsValid(MapPackage);
}

bool UTN_TravelFailureSubsystem::CanServerTravelTo(UWorld* World, const FString& URL, bool bAbsolute)
{
	const FWorldContext* Context = (GEngine && World) ? GEngine->GetWorldContextFromWorld(World) : nullptr;
	if (!Context)
	{
		return true; // Sin contexto no se puede resolver el mapa: decide el motor.
	}
	const FString MapPackage = TNTravel::TravelMapPackage(Context->LastURL, URL, bAbsolute);
	if (MapPackage.IsEmpty() || DoesTravelMapExist(MapPackage))
	{
		return true; // URL mala (la rechaza el motor) o mapa que existe.
	}

	const FString Error = FString::Printf(TEXT("el mapa «%s» no está en esta build (sin cocinar o mal escrito)"), *MapPackage);
	UGameInstance* GameInstance = World->GetGameInstance();
	UTN_TravelFailureSubsystem* Self = GameInstance ? GameInstance->GetSubsystem<UTN_TravelFailureSubsystem>() : nullptr;
	if (Self)
	{
		// Sin pasar por UEngine::OnTravelFailure: el viaje no ha empezado, así que no es una desconexión (el motor pediría el menú).
		Self->HandleTravelFailure(World, ETravelFailure::PackageMissing, Error);
	}
	else
	{
		GEngine->BroadcastTravelFailure(World, ETravelFailure::PackageMissing, Error);
	}
	return false;
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

	// El último fallo manda: lo que esperaba al fotograma siguiente por un fallo anterior ya no toca.
	CancelAfterFailure();

	const TNTravel::ETravelFailureAction Action = TNTravel::DecideTravelFailure(World ? World->GetNetMode() : NM_Standalone,
		GI->IsInMenuWorld(World), TNTravelFailureDetail::IsLobbyIntact(World, GI->LobbyReturnMapPath), FailureCount);
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
		// Tras un viaje duro fallido el anfitrión acaba aquí con su sesión de Steam viva y sin invitados (ver el .h).
		GI->DestroyCurrentSession();
		return;
	}
	if (Action == TNTravel::ETravelFailureAction::StayInLobby)
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[MP] El lobby sigue en pie: el anfitrión se queda en él con su sesión."));
		ScheduleAfterFailure(World, Action);
		return;
	}

	// Por si se acaba en el menú: allí sale el aviso (si el anfitrión llega al lobby, HandlePostLoadMap lo retira).
	LeaveMenuNotice(*GI);
	if (Action == TNTravel::ETravelFailureAction::ReturnHostToLobby && World)
	{
		ScheduleAfterFailure(World, Action);
		return;
	}
	SendToMenu(*GI, World);
}

void UTN_TravelFailureSubsystem::LeaveMenuNotice(UMP_GameInstance& GI)
{
	FTNMenuNotice Notice;
	Notice.Text = TNTravelFailureDetail::FailedNotice();
	Notice.bError = true;
	Notice.bOpenJoin = false;
	GI.SetMenuNotice(Notice);
	bNoticePending = true;
}

void UTN_TravelFailureSubsystem::ScheduleAfterFailure(UWorld* World, TNTravel::ETravelFailureAction Action)
{
	// Al empezar el fotograma siguiente (FCoreDelegates::OnBeginFrame): siempre después de UEngine::HandleTravelFailure y antes
	// del UEngine::TickWorldTravel que haría el viaje a «?closed», venga de donde venga el fallo. El ticker del núcleo no vale:
	// va después de UEngine::Tick, pero antes de los comandos diferidos (-ExecCmds), así que un fallo pedido desde ellos lo
	// encontraba ya pasado y el motor viajaba al menú antes.
	CancelAfterFailure();
	AfterFailureWorld = World;
	AfterFailureAction = Action;
	AfterFailureHandle = FCoreDelegates::OnBeginFrame.AddUObject(this, &UTN_TravelFailureSubsystem::HandleBeginFrameAfterFailure);
}

void UTN_TravelFailureSubsystem::HandleBeginFrameAfterFailure()
{
	UWorld* World = AfterFailureWorld.Get();
	CancelAfterFailure();

	UMP_GameInstance* GI = GetTNGameInstance();
	if (!GI || !World || World->GetGameInstance() != GI)
	{
		// El mundo ya no está: otro mapa ha ocupado su lugar y lo que se iba a hacer ya no tiene sentido.
		return;
	}
	TNTravelFailureDetail::KeepHostInSession(World);
	if (AfterFailureAction == TNTravel::ETravelFailureAction::ReturnHostToLobby)
	{
		TravelHostToLobby(*GI, *World);
	}
}

void UTN_TravelFailureSubsystem::TravelHostToLobby(UMP_GameInstance& GI, UWorld& World)
{
	// No en el acto: dentro de ProcessServerTravel World->NextURL aún no está vacío y ServerTravel no haría nada.
	if (World.IsInSeamlessTravel() || !World.NextURL.IsEmpty())
	{
		UE_LOG(LogTortunabo, Warning, TEXT("[MP] Ya hay otro viaje en marcha: no se pide el del lobby."));
		return;
	}

	const FString LobbyURL = TNTravel::LobbyTravelURL(GI.LobbyReturnMapPath);
	UE_LOG(LogTortunabo, Warning, TEXT("[MP] El anfitrión vuelve al lobby con todos: %s"), *LobbyURL);
	const int32 FailuresBefore = FailureCount;
	const bool bAccepted = World.ServerTravel(LobbyURL);
	if (FailureCount != FailuresBefore)
	{
		// El propio viaje al lobby ha fallado (un lobby sin cocinar): HandleTravelFailure ya ha actuado, y al ser el segundo
		// fallo seguido ha mandado al menú.
		return;
	}
	if (!TNTravel::DidTravelStart(bAccepted, World.IsInSeamlessTravel(), !World.NextURL.IsEmpty()))
	{
		UE_LOG(LogTortunabo, Error, TEXT("[MP] El viaje al lobby %s no ha arrancado: se oculta la pantalla de carga y se vuelve al menú."), *LobbyURL);
		GI.HideLoadingScreen();
		SendToMenu(GI, &World);
		return;
	}

	// Ya viaja: ahora sí, la pantalla de carga con el motivo.
	GI.ShowLoadingScreen(NSLOCTEXT("TNRooms", "TravelFailedLobby", "No se ha podido cargar la partida: volvéis al lobby.").ToString());
}

void UTN_TravelFailureSubsystem::CancelAfterFailure()
{
	FCoreDelegates::OnBeginFrame.Remove(AfterFailureHandle);
	AfterFailureHandle.Reset();
	AfterFailureWorld.Reset();
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
