#include "Multiplayer/TN_TravelFailureSubsystem.h"
#include "Multiplayer/MP_GameInstance.h"
#include "Multiplayer/TN_TravelFailureDecisions.h"
#include "Core/TN_Log.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectGlobals.h"

namespace TNTravelFailureDetail
{
	/** El aviso del menú tras un viaje fallido (también sirve para reconocerlo al retirarlo). */
	FText FailedNotice()
	{
		return NSLOCTEXT("TNRooms", "TravelFailed", "No se ha podido cargar la partida y has vuelto al menú.");
	}
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

	const ENetMode NetMode = World ? World->GetNetMode() : NM_Standalone;
	const TNTravel::ETravelFailureAction Action = TNTravel::DecideTravelFailure(NetMode, GI->IsInMenuWorld(World), FailureCount);
	++FailureCount;
	UE_LOG(LogTortunabo, Error, TEXT("[MP] Fallo de viaje %s: %s → %s (fallo %d seguido)."),
		ETravelFailure::ToString(FailureType), *ErrorString, TNTravel::ActionName(Action), FailureCount);

	// Nada de «Reconectando...»: el corte que venga ahora no es un ServerTravel normal.
	GI->ClearPendingTravel();
	GI->HideLoadingScreen();

	if (Action == TNTravel::ETravelFailureAction::StayInMenu)
	{
		GI->OnRoomNotice.Broadcast(TNTravelFailureDetail::FailedNotice(), true);
		return;
	}

	// Por si se acaba en el menú (el motor también vuelve al mapa por defecto si no hay otro viaje): allí sale el aviso.
	FTNMenuNotice Notice;
	Notice.Text = TNTravelFailureDetail::FailedNotice();
	Notice.bError = true;
	Notice.bOpenJoin = false;
	GI->SetMenuNotice(Notice);
	bNoticePending = true;

	if (Action == TNTravel::ETravelFailureAction::ReturnHostToLobby && World)
	{
		const FString LobbyURL = TNTravel::LobbyTravelURL(GI->LobbyReturnMapPath);
		GI->ShowLoadingScreen(NSLOCTEXT("TNRooms", "TravelFailedLobby", "No se ha podido cargar la partida: volvéis al lobby.").ToString());
		UE_LOG(LogTortunabo, Warning, TEXT("[MP] El anfitrión vuelve al lobby con todos: %s"), *LobbyURL);
		World->ServerTravel(LobbyURL);
		return;
	}

	GI->DestroyCurrentSession();
	if (APlayerController* PC = GI->GetFirstLocalPlayerController(World))
	{
		PC->ClientTravel(GI->GetMenuMapPath(), TRAVEL_Absolute);
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
